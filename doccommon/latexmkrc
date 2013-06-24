#
# latexmk doesn't know about glossaries by default.  Add a rule to map
# .glo files to .gls files via `makeglossaries`
#
add_cus_dep('glo', 'gls', 0, 'runMakeGlossary');
sub runMakeGlossary
{
	system("makeglossaries '$_[0]'");
}

