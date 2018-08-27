
tests:
	(cd ../linux; make type=debug)
	perl ./testall.pl -e

test_release:
	echo "Assuming release version build is up to date"
	perl ./testall.pl -e -r

test_installed:
	echo "Testing installed version"
	perl ./testall.pl -e -i
