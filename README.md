# git-depth

Computes depths of git revision graphs

## Motivation

In the days of Subversion, it was easy to derive version numbers from the Subversion revision number.
Ever since distributed version control became the norm, this is no longer possible.
Instead, a mechanism external to version control is typically used to allocate version numbers,
which makes builds irreproducible. 

This project proposes a different approach: the build number is derived from
the structure of the git commit graph. Therefore, repeated builds starting at the same
commit derive the same version number.

We guarantee that walking along a branch will produce strictly increasing
numbers, except when one of the user-specified files change.
In that case the version number resets.

Typically, this file will be a `VERSION` file containing the current major and minor version numbers.
The result of `git-depth` can then be used as a patch number
and appended to the contents of this file to produce a complete version.
Incrementing major or minor version will automatically reset patch to zero.

## Getting Started

1. Download binaries from [releases][1] and place them somewhere in your path.
2. Create your version file.

       $ echo 1.0 >VERSION
       $ git add VERSION
       $ git commit -m "Add VERSION file"

3. Use `git-depth` to calculate the version patch number.

       $ git-depth HEAD VERSION
       0

4. You can also automatically prepend the contents of the VERSION file.

       $ git-depth -c HEAD VERSION
       1.0.0
    
4. Add more commits.

       $ echo fix >>file.txt
       $ git commit -am "Fixes to file.txt"
       $ git-depth -c HEAD VERSION
       1.0.1
       $ echo the previous fix did not work >>file.txt
       $ git commit -am "More fixes to file.txt"
       $ git-depth -c HEAD VERSION
       1.0.2
    
5. Increase the minor version and observe the patch number reset.

       $ echo 1.1 >VERSION
       $ git commit -am "Bump version to 1.1"
       $ git-depth -c HEAD VERSION
       1.1.0

## License

The project itself is licensed under the MIT license.
However, the released binaries embed libgit2, which is licensed under GPL,
and as such the binary releases are licensed as GPL a well.

  [1]: https://github.com/avast/git-depth/releases
