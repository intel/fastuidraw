clean:
	rm -fr $(CLEAN_FILES)

clean-all: clean
	rm -fr $(SUPER_CLEAN_FILES)

clean-docs:
	rm -r docs/doxy/html

TARGETLIST+=clean clean-docs clean-all
