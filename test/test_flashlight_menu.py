#!/usr/bin/env python3
"""Compile the production Home index mapping and verify optional service/light rows."""
from pathlib import Path
import re,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
home=(root/'src/activities/home/HomeActivity.h').read_text()
manager=(root/'src/activities/ActivityManager.h').read_text()
enum=re.search(r'enum class HomeMenuItem\s*\{[^}]*\};',manager).group()
functions='\n'.join(re.search(r'  static (?:int|HomeMenuItem) '+name+r'\([\s\S]*?^  }',home,re.M).group() for name in ['menuItemToIndex','indexToMenuItem'])
main=r'''
int main() {
 for(bool service:{false,true}) for(bool light:{false,true}) {
  std::vector<HomeMenuItem> expected={HomeMenuItem::FILE_BROWSER,HomeMenuItem::LIBRARY};
  if(service) expected.push_back(HomeMenuItem::OPDS_BROWSER);
  expected.push_back(HomeMenuItem::FILE_TRANSFER);expected.push_back(HomeMenuItem::SETTINGS_MENU);
  if(light) expected.push_back(HomeMenuItem::FLASHLIGHT);
  for(unsigned i=0;i<expected.size();++i){assert(menuItemToIndex(expected[i],service,light)==static_cast<int>(i));assert(indexToMenuItem(i,service,light)==expected[i]);}
  assert(indexToMenuItem(expected.size(),service,light)==HomeMenuItem::NONE);
  if(!light) assert(menuItemToIndex(HomeMenuItem::FLASHLIGHT,service,light)==0);
 }
 std::puts("Production Home mapping: all optional service/flashlight combinations passed");
}
'''
with tempfile.TemporaryDirectory(prefix='flashlight-menu-') as tmp:
 source=Path(tmp)/'test.cpp';binary=Path(tmp)/'test'
 source.write_text('#include <cassert>\n#include <cstdio>\n#include <vector>\n'+enum+'\n'+functions+main)
 subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror',str(source),'-o',str(binary)],check=True)
 subprocess.run([str(binary)],check=True)
