#include <git2.h>
#include <CLI/CLI.hpp>
#include <algorithm>
#include <iostream>
#include <map>
#include <string>
#include <string.h>
#include <string_view>
#include <utility>

using namespace std::string_view_literals;

constexpr unsigned char nulloid[20] = {};

struct oid_ref
{
	oid_ref()
		: ptr(nulloid)
	{
	}

	oid_ref(git_oid const & o)
		: ptr(o.id)
	{
	}

	friend bool operator==(oid_ref lhs, oid_ref rhs)
	{
		return memcmp(lhs.ptr, rhs.ptr, 20) == 0;
	}

	friend bool operator!=(oid_ref lhs, oid_ref rhs)
	{
		return !(lhs == rhs);
	}

	friend bool operator<(oid_ref lhs, oid_ref rhs)
	{
		return memcmp(lhs.ptr, rhs.ptr, 20) < 0;
	}

	unsigned char const * ptr;
};

struct _git2_error_checker
{
	_git2_error_checker(char const * file, int line)
		: _file(file), _line(line)
	{
	}

	void operator%(int err)
	{
		if (err < 0)
			throw std::runtime_error(_file + std::string("(") + std::to_string(_line) + "): git operation failed: " + std::to_string(err));
	}

	char const * _file;
	int _line;
};

#define git2_try _git2_error_checker{__FILE__, __LINE__} %

template <typename F>
struct _deferred
{
	_deferred(F && f)
		: _f(std::forward<F>(f))
	{
	}

	~_deferred()
	{
		_f();
	}

	std::remove_reference_t<F> _f;
};

#define PP_CAT2(a, b) a ## b
#define PP_CAT(a, b) PP_CAT2(a, b)

#define defer _deferred PP_CAT(_deferred_, __COUNTER__) = [&]()

int main(int argc, char * argv[])
{
	try
	{
		std::string commit = "HEAD";
		std::vector<std::string> paths;
		bool cat = false;

		CLI::App ap{ "git-depth" };
		ap.add_flag("-c,--cat", cat);
		ap.add_option("commit", commit);
		ap.add_option("path", paths);
		CLI11_PARSE(ap, argc, argv);

		git2_try git_libgit2_init();
		defer{ git_libgit2_shutdown(); };

		git_repository * repo = nullptr;
		git2_try git_repository_open_ext(&repo, ".", 0, "");
		defer{ git_repository_free(repo); };

		auto resolve_blob = [&](git_oid & oid, git_commit * commit, char const * path) -> bool {
			git_tree * tree;
			git2_try git_commit_tree(&tree, commit);
			defer{ git_tree_free(tree); };

			git_tree_entry * entry;
			int err = git_tree_entry_bypath(&entry, tree, path);
			if (err == GIT_ENOTFOUND)
				return false;
			git2_try err;
			defer{ git_tree_entry_free(entry); };

			if (git_tree_entry_type(entry) != GIT_OBJECT_BLOB)
				return false;

			oid = *git_tree_entry_id(entry);
			return true;
		};

		git_object * obj;
		git2_try git_revparse_single(&obj, repo, commit.c_str());
		defer{ git_object_free(obj); };

		if (git_object_type(obj) != GIT_OBJECT_COMMIT)
		{
			std::cerr << "error: invalid commitish specified\n";
			return 4;
		}

		git_commit * root_commit = (git_commit *)obj;
		git_oid const * root_oid = git_commit_id(root_commit);

		std::vector<git_oid> root_blob_oids;
		for (std::string const & path: paths)
		{
			git_oid oid;
			if (!resolve_blob(oid, root_commit, path.c_str()))
			{
				std::cerr << "error: the file was not found in the repository: " << path << "\n";
				return 5;
			}

			root_blob_oids.push_back(oid);
		}

		std::string version_prefix;
		if (cat)
		{
			for (git_oid const & oid: root_blob_oids)
			{
				git_blob * blob;
				git2_try git_blob_lookup(&blob, repo, &oid);
				defer{ git_blob_free(blob); };

				version_prefix.append((char const *)git_blob_rawcontent(blob), git_blob_rawsize(blob));
				while (!version_prefix.empty() && " \t\r\n"sv.find(version_prefix.back()) != std::string_view::npos)
					version_prefix.pop_back();

				version_prefix.push_back('.');
			}
		}

		std::map<git_oid, size_t, std::less<oid_ref>> boundary;
		boundary[*root_oid] = 0;

		git_revwalk * walk;
		git2_try git_revwalk_new(&walk, repo);
		defer{ git_revwalk_free(walk); };

		git2_try git_revwalk_sorting(walk, GIT_SORT_TOPOLOGICAL);
		git2_try git_revwalk_push(walk, root_oid);

		size_t total_depth = 0;
		git_oid bottom_commit_oid = *root_oid;

		git_oid commit_oid;
		while (!boundary.empty())
		{
			int walk_err = git_revwalk_next(&commit_oid, walk);
			if (walk_err == GIT_ENOTFOUND)
			{
				std::cerr << "error: cannot complete the graph walk; perhaps the repo is too shallow?\n";
				return 6;
			}
			git2_try walk_err;

			auto it = boundary.find(commit_oid);
			if (it == boundary.end())
				continue;

			auto depth = it->second;
			boundary.erase(it);

			git_commit * commit;
			git2_try git_commit_lookup(&commit, repo, &commit_oid);
			defer{ git_commit_free(commit); };

			bool hidden = false;
			for (size_t i = 0; !hidden && i != paths.size(); ++i)
			{
				git_oid oid;
				if (!resolve_blob(oid, commit, paths[i].c_str())
					|| oid_ref(oid) != root_blob_oids[i])
				{
					hidden = true;
				}
			}

			if (hidden)
				continue;

			if (total_depth < depth)
			{
				total_depth = depth;
				bottom_commit_oid = commit_oid;
			}

			auto parent_count = git_commit_parentcount(commit);

			for (decltype(parent_count) i = 0; i != parent_count; ++i)
			{
				auto parent_oid = git_commit_parent_id(commit, i);
				auto & parent_depth = boundary[*parent_oid];

				parent_depth = (std::max)(parent_depth, depth + 1);
			}
		}

		std::cout << version_prefix << total_depth << "\n";
		return 0;
	}
	catch (std::exception const & e)
	{
		std::cerr << "error: " << e.what() << "\n";
		return 3;
	}
}
