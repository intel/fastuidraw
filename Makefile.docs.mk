docs: docs/doxy/html/index.html
.PHONY: docs
docs/doxy/html/index.html:
	-cd docs && doxygen Doxyfile
TARGETLIST += docs
CLEAN_FILES += docs/doxy/html

clean-docs:
	rm -r docs/doxy/html
TARGETLIST += clean-docs
