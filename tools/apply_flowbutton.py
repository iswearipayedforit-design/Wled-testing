from pathlib import Path

root = Path(__file__).resolve().parents[1]
wled = root / "WLED"
userlist = wled / "wled00" / "usermods_list.cpp"
source = userlist.read_text(encoding="utf-8")

include_anchor = '#ifdef USERMOD_AUDIOREACTIVE\n  #include "../usermods/audioreactive/audio_reactive.h"\n#endif\n'
include_block = include_anchor + '\n#ifdef USERMOD_FLOWBUTTON\n  #include "../usermods/FlowButton/FlowButton.h"\n#endif\n'

register_anchor = '#ifdef USERMOD_AUDIOREACTIVE\n  UsermodManager::add(new AudioReactive());\n  #endif\n'
register_block = register_anchor + '\n  #ifdef USERMOD_FLOWBUTTON\n  UsermodManager::add(new FlowButton());\n  #endif\n'

if 'USERMOD_FLOWBUTTON' not in source:
    if include_anchor not in source:
        raise SystemExit('AudioReactive include anchor not found in WLED 0.15.1 usermods_list.cpp')
    if register_anchor not in source:
        raise SystemExit('AudioReactive register anchor not found in WLED 0.15.1 usermods_list.cpp')
    source = source.replace(include_anchor, include_block, 1)
    source = source.replace(register_anchor, register_block, 1)
    userlist.write_text(source, encoding='utf-8')

flow_src = root / 'usermods' / 'FlowButton'
flow_dst = wled / 'usermods' / 'FlowButton'
flow_dst.mkdir(parents=True, exist_ok=True)
(flow_dst / 'FlowButton.h').write_text((flow_src / 'FlowButton.h').read_text(encoding='utf-8'), encoding='utf-8')

print('FlowButton applied to WLED source tree.')
