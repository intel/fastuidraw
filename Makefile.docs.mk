docs: docs/doxy/html/index.html
.PHONY: docs
docs/doxy/html/index.html:
	-cd docs && doxygen Doxyfile
TARGETLIST += docs
CLEAN_FILES += docs/doxy/html
