# Function that adds a target name to the help output target
define addtargetname
$(eval
TARGETLIST += " $(strip $(1))"
)
endef

TARGETLIST :=

# Function that expands to the list of files with the correct path
filelist = $(foreach filename,$(1),$(d)/$(filename))