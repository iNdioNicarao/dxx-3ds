all: d1

d1:
	@$(MAKE) -C d1

dist-bin: d1
	@$(MAKE) -C d1 dist-bin

clean:
	@$(MAKE) -C d1 clean

.PHONY: d1
