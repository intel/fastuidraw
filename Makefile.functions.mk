# Function that expands to the list of files with the correct path
filelist = $(foreach filename,$(1),$(d)/$(filename))
