#
if ($#argv < 1) then
	echo 'Usage: install <-me file list>'
	exit
endif
echo stripping and installing $*
foreach i ($*)
	echo ${1}:
	ed $i << 'EOF'
1a
%beginstrip%
.
g/%beginstrip%/d
i
.\" This version has had comments stripped; an unstripped version is available.
.
+,$g/[.	]\\".*/s///
g/[ 	][ 	]*$/s///
g/^$/d
g/\\n@/d
w _mac_temp_
q
'EOF'
	if ($i == tmac.e) then
		cp _mac_temp_ /usr/lib/tmac.e
	else
		cp _mac_temp_ /usr/lib/me/$i
	endif
	rm _mac_temp_
end
cp revisions /usr/lib/me/revisions
echo	"Done"
exit
