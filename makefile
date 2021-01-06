#

help: 
	@echo "To build a debian package for SNAP use eg:"
	@echo "> DISTRIBUTION=focal make package"

package:
	build/build_docker.sh
	build/build_package.sh
