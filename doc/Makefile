# Copyright (c) 2024 Henrik Brix Andersen <henrik@brixandersen.dk>
# SPDX-License-Identifier: Apache-2.0
#
# Minimal Makefile for Sphinx documentation

SPHINXOPTS    ?=
SPHINXBUILD   ?= sphinx-build

SOURCEDIR     = .
BUILDDIR      = build

.PHONY: Makefile clean help html

html: Makefile
	@$(SPHINXBUILD) -M $@ "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)

help:
	@$(SPHINXBUILD) -M help "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)

clean:
	$(RM) -r build
