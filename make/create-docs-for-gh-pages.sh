#!/bin/sh

##########################################
# This is a silly script to build the doxygen
# documentation and push the docs to gh-pages
# branch. Note that the script checks out master
# and builds the docs from master AND the branch
# gh-pages has the docs located in docs/html while
# the docs are built to docs/doxy/html.
if ! git diff-files --quiet --ignore-submodules --
then
    echo >&2 "There are unstaged changes, not updating docs to gh-pages"
    git diff-files --name-status -r --ignore-submodules -- >&2
    exit 1
fi

# Disallow uncommitted changes in the index
if ! git diff-index --cached --quiet HEAD --ignore-submodules --
then
    echo >&2 "There are uncommitted changes, not updating docs to gh-pages"
    git diff-index --cached --name-status -r --ignore-submodules HEAD -- >&2
    exit 1
fi

git checkout master
make clean-docs
make docs
git checkout gh-pages
git checkout master README.md
git rm -rf docs/html
rm -rf docs/html
mv docs/doxy/html docs/html
git add docs/html
git commit -nm "Update content"
git push
rm -r docs/html
git checkout master
