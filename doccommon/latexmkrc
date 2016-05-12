#
# latexmk doesn't know about glossaries by default.  Add a rule to map
# .glo files to .gls files via `makeglossaries`
#
add_cus_dep('glo', 'gls', 0, 'runMakeGlossary');
sub runMakeGlossary
{
	system("makeglossaries '$_[0]'");
}

# Remove bibliography intermediaries (bbl brf)
# Remove draft watermark (xwm)
$clean_ext = "bbl brf xwm"

