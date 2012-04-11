# This script matches a line ending with "kernel.bsd</key>".
# If the match is found, the next line is read and the version changed to "6.0".
/kernel\.bsd<\/key>$/ {
	N
	/>[.0-9]*</ {
		s//>6.0</
	}
}