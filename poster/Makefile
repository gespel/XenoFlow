# ===============================
# Configuration
# ===============================

MAIN    := poster
OUTDIR  := build
LATEXMK := latexmk
FLAGS   := -pdf -interaction=nonstopmode -halt-on-error -shell-escape

# ===============================
# Default target
# ===============================

all: $(MAIN).pdf

$(MAIN).pdf: $(MAIN).tex
	@mkdir -p $(OUTDIR)
	$(LATEXMK) $(FLAGS) -output-directory=$(OUTDIR) $<
	@cp $(OUTDIR)/$(MAIN).pdf .

clean:
	$(LATEXMK) -C
	@rm -rf $(OUTDIR)

rebuild: clean all

.PHONY: all clean rebuild

