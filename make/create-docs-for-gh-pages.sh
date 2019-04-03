#!/bin/sh

##########################################
# This is a silly script to build the doxygen
# documentation and push the docs to gh-pages
# branch. Note that the script checks out master
# and builds the docs from master AND the branch
# gh-pages has the docs located in docs/html while
# the docs are built to docs/doxy/html.

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
git checkout master
