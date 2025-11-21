file(REMOVE_RECURSE
  "liblibicache.a"
  "liblibicache.pdb"
)

# Per-language clean rules from dependency scanning.
foreach(lang C)
  include(CMakeFiles/libicache.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
