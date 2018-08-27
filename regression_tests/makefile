
tests:
	(cd ../linux; make type=debug)
	perl ./testall.pl -e

test_release:
	echo "Assuming release version build is up to date\n"
	perl ./testall.pl -e -r
