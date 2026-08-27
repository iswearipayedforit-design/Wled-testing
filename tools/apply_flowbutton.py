from pathlib import Path

root = Path(__file__).resolve().parents[1]
wled = root / "WLED"
userlist = wled / "wled00" / "usermods_list.cpp"
source = userlist.read_text(encoding="utf-8")

include_anchor = '#ifdef USERMOD_AUDIOREACTIVE\n  #include "../usermods/audioreactive/audio_reactive.h"\n#endif\n'
include_block = include_anchor + '''
#ifdef USERMOD_FLOWBUTTON
  #include "../usermods/FlowButton/FlowButton.h"
#endif

#ifdef USERMOD_SMOOTHBUTTON
  #include "../usermods/SmoothButton/SmoothButton.h"
#endif

#ifdef USERMOD_TOUCHBUTTON
  #include "../usermods/TouchButton/TouchButton.h"
#endif
'''

register_anchor = '#ifdef USERMOD_AUDIOREACTIVE\n  UsermodManager::add(new AudioReactive());\n  #endif\n'
register_block = register_anchor + '''
  #ifdef USERMOD_FLOWBUTTON
  UsermodManager::add(new FlowButton());
  #endif

  #ifdef USERMOD_SMOOTHBUTTON
  UsermodManager::add(new SmoothButton());
  #endif

  #ifdef USERMOD_TOUCHBUTTON
  UsermodManager::add(new TouchButton());
  #endif
'''

if include_anchor not in source:
    raise SystemExit('AudioReactive include anchor not found in WLED 0.15.1 usermods_list.cpp')
if register_anchor not in source:
    raise SystemExit('AudioReactive register anchor not found in WLED 0.15.1 usermods_list.cpp')

source = source.replace(include_anchor, include_block, 1)
source = source.replace(register_anchor, register_block, 1)
userlist.write_text(source, encoding='utf-8')

for name in ('FlowButton', 'SmoothButton', 'TouchButton'):
    src = root / 'usermods' / name
    dst = wled / 'usermods' / name
    dst.mkdir(parents=True, exist_ok=True)
    header = f'{name}.h'
    (dst / header).write_text((src / header).read_text(encoding='utf-8'), encoding='utf-8')

print('FlowButton, SmoothButton and TouchButton applied to WLED source tree.')
