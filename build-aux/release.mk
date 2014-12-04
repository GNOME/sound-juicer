# Copyright (C) 2014 Phillip Wood <phillip.wood@dunelm.org.uk>
#
# Sound Juicer - release.mk
#
# This makefile is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, see <http://www.gnu.org/licenses/>.
#
# Authors: Phillip Wood <phillip.wood@dunelm.org.uk>

# Run pre-release checks
# Push to origin then create a signed tag and push it if the initial push
# was successful, then upload the tarball and install it.
release: release-check distcheck
	@yes_no() { \
	  printf '%s' "$$1"; \
	  while true; \
	  do \
	    read response; \
	    case "$$response" in \
	      ( y | Y | yes | YES | Yes ) break ;; \
	      ( n | N | no | NO | No ) exit 1 ;; \
	    esac ; \
	    printf "Please answer 'y' or 'n' "; \
	  done; \
	}; \
	branch=$$(git symbolic-ref --short HEAD); \
	printf "Checking what would be pushed"; \
	pushed=$$(git push --dry-run --porcelain origin $$branch |grep  '^[- *+=!]'); \
	case "$${pushed%%	*}" in \
	  ( ' ' ) tmp="$${pushed#*	}"; \
		  refs="$${tmp%%	*}"; revs="$${tmp#*	}"; \
		  printf " - $$refs\n"; \
		  git log --oneline --decorate $$revs; \
		  yes_no 'Do you want to push these commits and a tag for release $(VERSION)? '; \
		  git push origin $$branch || exit 1;; \
	  ( '*' ) printf " - new branch $$branch\n"; \
		  yes_no "Do you want to push a new branch '$$branch' and a tag for release $(VERSION)? "; \
		  git push origin $$branch || exit 1;; \
	  ( '=' ) printf " - nothing\n"; \
		  yes_no 'Do you want to create and push a tag for release $(VERSION)? ' ;; \
	  ( * ) echo "$$pushed"; exit 1;; \
	esac; \
	echo 'Creating tag $(VERSION)'; \
	git tag -s -m 'Release $(VERSION)' '$(VERSION)' || exit 1; \
	git push origin tag '$(VERSION)' || exit 1; \
	yes_no 'Do you want to upload $(PACKAGE)-$(VERSION).tar.xz to master.gnome.org? '; \
	scp '$(PACKAGE)-$(VERSION).tar.xz' 'master.gnome.org:' || exit 1; \
	ssh master.gnome.org ftpadmin install '$(PACKAGE)-$(VERSION).tar.xz'

# Check that the version number is a.b.c[.d[...]]
# Check that we're on the correct branch
#  - master for development releases and stable a.b.0 stable releases
#  - gnome-a-b for a.b.c[.d] stable updates if it exists, master if not
# Check the working tree matches the index
# Check the commit message of the release
# Check that there are no other releases with this version on the current branch
# Check that there is no tag for this version
release-check:
	@echo "Running pre-release checks"; \
	branch=$$(git symbolic-ref --short HEAD); \
	v=$(VERSION); v1=$${v%%.*}; v2=$${v#*.}; v3=$${v2#*.}; v4=$${v3#*.}; \
	test -z "$$v" -o "$$v" == "$$v1" -o "$$v1" = "$$v2" -o "$$v2" = "$$v3" && \
	  { echo "Error: Version '$(VERSION)' is too short"; exit 1; }; \
	if test "$$v3" = "$$v4"; then \
	    v4=; \
	else \
	    v4=$${v4%%.*}; \
	fi; \
	v2=$${v2%%.*}; v3=$${v3%%.*}; \
	stable_branch="gnome-$$v1-$$v2"; \
	b=$$(git branch --list $$stable_branch); \
	odd=$$(echo $$v2 | grep '[13579]$$'); \
	expected_branch="master"; \
	test -z "$$odd" -a ! -z "$$b" && test ! -z "$$v4" -o "$$v3" != "0" && \
	    expected_branch=$$stable_branch; \
	test "$$branch" = "$$expected_branch" || \
	  { echo "Error: Current branch is '$$branch' but expected \
$(PACKAGE)-$(VERSION) to be released from branch '$$expected_branch'."; \
	    exit 1; }; \
	test -z "$$(git status --porcelain -uno)" || \
	  { echo "Error: There are uncommited changes"; \
	    git status -uno; exit 1; }; \
	x=$$(git log -n1 --format=%s); \
	test "$$x" = "Release $(VERSION)" || \
	  { echo "Error: Last commit should be 'Release $(VERSION)'"; \
	    git log -n1 --oneline; exit 1; }; \
	x=$$(git log --oneline --grep $(VERSION) -F HEAD^ | \
	  grep -i '^[0-9a-f]*[ 	]*release[ 	]*$(subst .,\.,$(VERSION))[ 	]*$$'); \
	test -z "$$x" || \
	  { echo "Error: Release $(VERSION) already exists"; \
	    git log -n1 --oneline --decorate "$${x%% *}"; exit 1; }; \
	x=$$(git tag --list $(VERSION)); \
	test -z "$$x" || \
	  { echo 'Error: Tag $(VERSION) already exists'; \
	    git log -n1 --oneline --decorate tags/$(VERSION); exit 1; }; \
	echo "Pre-release checks passed"

.PHONY: release release-check
