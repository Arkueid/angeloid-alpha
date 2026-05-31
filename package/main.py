import sys
from pathlib import Path

import glfw
import OpenGL
OpenGL.ERROR_CHECKING = False
import OpenGL.GL as gl


sys.path.insert(0, str(Path(__file__).parent))
from angeloid import init, glInit, dispose, Model, Camera

MODELS = {
    "ikaros-origin": "resources/models/ikaros-origin/Ikaros.pmx",
    "ikaros-uniform": "resources/models/ikaros-uniform/Ikaros.pmx",
    "安比": "resources/models/安比/安比.pmx",
    "刀": "resources/models/安比/刀.pmx",
    "chloe": "resources/models/Chloe_Uniform1_0.9/Chloe_Uniform1_0.9.pmx",
    "aqua-swimwear": "resources/models/Aqua_Swimwear_1.0/Aqua_Swimwear_1.0.pmx",
    "marine-swimwear": "resources/models/Marine_Swmwear_1.01/Marine_Swmwear_1.01.pmx",
    "aqua-basebody": "resources/models/Aqua_BaseBody_R15_0.9/Aqua_BaseBody_R15_0.9.pmx",
    "aqua-sailor": "resources/models/Aqua_Sailor_0.8/Aqua_Sailor_0.8.pmx",
    "brujas": "resources/models/Brujas/Brujas.pmx",
    "lamy-swimwear": "resources/models/Lamy_Swimwear_1.0/Lamy_Swimwear_1.0.pmx",
    "lulum": "resources/models/lulum/lulum_1.0.pmx",
    "marine-jk1": "resources/models/Marine_JK1_Set_1.01/Marine_JK1_1.0.pmx",
    "marine-jk1-hi": "resources/models/Marine_JK1_Set_1.01/Marine_JK1_Hi_1.0.pmx",
    "rurudo-lion": "resources/models/RurudoLion_1.0/RurudoLion_1.0.pmx",
    "rurudo-lion-hi": "resources/models/RurudoLion_1.0/RurudoLion_Hi_1.0.pmx",
    "卢西娅": "resources/models/卢西娅/卢西娅.pmx",
    "卢西娅-摘帽": "resources/models/卢西娅/卢西娅_摘帽.pmx",
    "卢西娅-武器1": "resources/models/卢西娅/武器1.pmx",
    "卢西娅-武器2": "resources/models/卢西娅/武器2.pmx",
    "伊里伽尔": "resources/models/伊里伽尔/伊里伽尔.pmx",
    "伊里伽尔-redhat": "resources/models/伊里伽尔-redhat/童话式复古.pmx",
    "姵儿": "resources/models/姵儿/椛暗式-姵儿ver1.2.pmx",
    "艾尔莎": "resources/models/艾尔莎/莎小姐.pmx",
}

PROJ_ROOT = Path(__file__).parent.parent

def print_help():
    print("""
FPS Camera Controls:
  Left mouse drag: Rotate camera view
  W/A/S/D: Move forward/left/backward/right
  E/Q: Move up/down
  Mouse scroll: Adjust movement speed
  X key: Toggle world axis display
  B key: Toggle rigidbody & joint display
  H key: Toggle model mesh display
  O key: Toggle outline display
  T key: Toggle toon shading
  K key: Toggle GPU skinning
  P key: Toggle VPD pose
  R key: Reset camera to default position
  I key: Toggle idle animation
  < / > keys: Switch between morphs
  Up/Down keys: Adjust morph weight
VMD Animation Controls:
  Space: Play/Pause VMD animation
  L key: Toggle VMD loop
  [ / ] keys: Step backward/forward 30 frames
""")

def main():
    print("MMD PMX Viewer (Python)")
    
    import argparse
    parser = argparse.ArgumentParser(description="MMD PMX Viewer")
    parser.add_argument("--model", "-m", nargs="?", default="艾尔莎", help="Model name or path")
    parser.add_argument("--vmd", "-v", nargs="*", help="VMD animation files")
    args = parser.parse_args()
    
    model_name = args.model
    vmd_paths = args.vmd or []
    
    pmx_path = Path(MODELS.get(model_name, model_name))
    vpd_path = Path("resources/vpd/自然站姿.vpd")
    
    if not pmx_path.is_absolute():
        pmx_path = PROJ_ROOT / pmx_path
    if not vpd_path.is_absolute():
        vpd_path = PROJ_ROOT / vpd_path
    
    if not glfw.init():
        print("Failed to initialize GLFW")
        return
    
    glfw.window_hint(glfw.CONTEXT_VERSION_MAJOR, 3)
    glfw.window_hint(glfw.CONTEXT_VERSION_MINOR, 3)
    glfw.window_hint(glfw.OPENGL_PROFILE, glfw.OPENGL_CORE_PROFILE)
    
    window = glfw.create_window(1280, 720, "MMD PMX Viewer", None, None)
    if not window:
        glfw.terminate()
        print("Failed to create window")
        return
    
    glfw.make_context_current(window)
    
    glInit()
    
    shader_dir = PROJ_ROOT / "resources/shaders"
    toon_dir = PROJ_ROOT / "resources/toon"
    blink_morphs = ["blink", "blink_l", "blink_r", "まばたき", "まぶたき", "ウィンク", "ｳｨﾝｸ"]
    
    init(str(shader_dir), str(toon_dir), blink_morphs)
    
    model = Model()
    model.load(str(pmx_path))
    glfw.set_window_title(window, f"MMD PMX Viewer - {model.modelName()}")
    
    active_vpd_id = -1
    if vpd_path.exists():
        active_vpd_id = model.loadVpd(str(vpd_path))
        if active_vpd_id >= 0:
            model.applyVpd(active_vpd_id)
    
    vmd_track_ids = []
    for vp in vmd_paths:
        vp = Path(vp)
        if not vp.is_absolute():
            vp = PROJ_ROOT / vp
        if vp.exists():
            track_id = model.loadVmd(str(vp))
            vmd_track_ids.append(track_id)
            if track_id >= 0:
                def on_vmd_end(tid):
                    print(f"VMD track {tid} finished playing.")
                    model.syncVpdPose()
                model.playVmd(track_id, on_vmd_end)
    
    print_help()
    
    morph_list = model.interactableMorphs()
    morph_index = -1
    morph_weight = 0.0
    
    cam = Camera()
    
    show_axis = True
    show_rigid = False
    
    def on_mouse_button(win, button, action, mods):
        if button == glfw.MOUSE_BUTTON_LEFT:
            cam.onMouseButton(action == glfw.PRESS)
    
    def on_cursor_pos(win, x, y):
        cam.onCursorPos(x, y)
    
    def on_scroll(win, xo, yo):
        cam.onScroll(yo)
    
    def on_key(win, key, scancode, action, mods):
        nonlocal morph_index, morph_weight, show_axis, show_rigid, active_vpd_id
        
        if key == glfw.KEY_ESCAPE and action == glfw.PRESS:
            glfw.set_window_should_close(window, True)
        
        if key == glfw.KEY_X and action == glfw.PRESS:
            show_axis = not show_axis
            print(f"World axis: {'ON' if show_axis else 'OFF'}")
        
        if key == glfw.KEY_B and action == glfw.PRESS:
            show_rigid = not show_rigid
            model.showRigidBodies(show_rigid)
            print(f"Rigid bodies: {'ON' if show_rigid else 'OFF'}")
        
        if key == glfw.KEY_H and action == glfw.PRESS:
            model.showModel(not model.getShowModel())
            print(f"Model: {'ON' if model.getShowModel() else 'OFF'}")
        
        if key == glfw.KEY_O and action == glfw.PRESS:
            model.showOutline(not model.getShowOutline())
            print(f"Outline: {'ON' if model.getShowOutline() else 'OFF'}")
        
        if key == glfw.KEY_T and action == glfw.PRESS:
            model.showToon(not model.getShowToon())
            print(f"Toon: {'ON' if model.getShowToon() else 'OFF'}")
        
        if key == glfw.KEY_K and action == glfw.PRESS:
            model.setSkinning(not model.isSkinned())
            print(f"GPU skinning: {'ON' if model.isSkinned() else 'OFF'}")
        
        if key == glfw.KEY_Y and action == glfw.PRESS:
            model.enablePhysics(not model.physicsEnabled())
            print(f"Physics: {'ON' if model.physicsEnabled() else 'OFF'}")
        
        if key == glfw.KEY_R and action == glfw.PRESS:
            cam.reset()
            print("Camera reset")
        
        if key == glfw.KEY_I and action == glfw.PRESS:
            idle = not model.morphCount() > 0
            model.setIdleBlink(idle)
            if not idle:
                model.clearMorphs()
            print(f"Idle blink: {'ON' if idle else 'OFF'}")
        
        if key == glfw.KEY_P and action == glfw.PRESS:
            if active_vpd_id >= 0 and model.vpdApplied():
                model.resetPose()
                print("VPD pose: OFF")
            elif active_vpd_id >= 0:
                model.applyVpd(active_vpd_id)
                print("VPD pose: ON")
        
        if key == glfw.KEY_COMMA and action == glfw.PRESS:
            if morph_list:
                idx = morph_list.index(morph_index) if morph_index in morph_list else 0
                idx = (idx - 1) % len(morph_list)
                morph_index = morph_list[idx]
                name = model.morphName(morph_index)
                morph_weight = model.savedMorphWeight(name)
                model.setMorphWeight(name, morph_weight)
                print(f"Morph: {name} = {morph_weight:.2f}")
        
        if key == glfw.KEY_PERIOD and action == glfw.PRESS:
            if morph_list:
                idx = morph_list.index(morph_index) if morph_index in morph_list else 0
                idx = (idx + 1) % len(morph_list)
                morph_index = morph_list[idx]
                name = model.morphName(morph_index)
                morph_weight = model.savedMorphWeight(name)
                model.setMorphWeight(name, morph_weight)
                print(f"Morph: {name} = {morph_weight:.2f}")
        
        if key == glfw.KEY_UP and action != glfw.RELEASE:
            if 0 <= morph_index < model.morphCount():
                morph_weight = min(1.0, morph_weight + 0.1)
                name = model.morphName(morph_index)
                model.setMorphWeight(name, morph_weight)
                print(f"Morph: {name} = {morph_weight:.2f}")
        
        if key == glfw.KEY_DOWN and action != glfw.RELEASE:
            if 0 <= morph_index < model.morphCount():
                morph_weight = max(0.0, morph_weight - 0.1)
                name = model.morphName(morph_index)
                model.setMorphWeight(name, morph_weight)
                print(f"Morph: {name} = {morph_weight:.2f}")
        
        if key == glfw.KEY_SPACE and action == glfw.PRESS:
            if vmd_track_ids:
                if model.isVmdPlaying():
                    model.pauseAllVmd()
                    print("VMD: PAUSED")
                else:
                    model.playAllVmd()
                    print("VMD: PLAYING")
        
        if key == glfw.KEY_L and action == glfw.PRESS:
            if vmd_track_ids:
                model.stopAllVmd()
                model.playAllVmd()
                print("VMD: RESTARTED")
        
        if key == glfw.KEY_LEFT_BRACKET and action != glfw.RELEASE:
            for tid in vmd_track_ids:
                frame = model.vmdCurrentFrame(tid) - 30
                model.setVmdFrame(tid, frame)
        
        if key == glfw.KEY_RIGHT_BRACKET and action != glfw.RELEASE:
            for tid in vmd_track_ids:
                frame = model.vmdCurrentFrame(tid) + 30
                model.setVmdFrame(tid, frame)
    
    glfw.set_mouse_button_callback(window, on_mouse_button)
    glfw.set_cursor_pos_callback(window, on_cursor_pos)
    glfw.set_scroll_callback(window, on_scroll)
    glfw.set_key_callback(window, on_key)
    
    last_time = glfw.get_time()
    frame_count = 0
    fps_time = 0.0
    
    while not glfw.window_should_close(window):
        current_time = glfw.get_time()
        dt = current_time - last_time
        last_time = current_time
        
        w = glfw.get_key(window, glfw.KEY_W) == glfw.PRESS
        a = glfw.get_key(window, glfw.KEY_A) == glfw.PRESS
        s = glfw.get_key(window, glfw.KEY_S) == glfw.PRESS
        d = glfw.get_key(window, glfw.KEY_D) == glfw.PRESS
        e = glfw.get_key(window, glfw.KEY_E) == glfw.PRESS
        q = glfw.get_key(window, glfw.KEY_Q) == glfw.PRESS
        
        cam.update(dt, w, a, s, d, e, q)
        model.update(dt)
        
        frame_count += 1
        fps_time += dt
        if fps_time >= 0.5:
            fps = int(frame_count / fps_time)
            glfw.set_window_title(window, f"MMD PMX Viewer - {model.modelName()} [{fps} FPS]")
            frame_count = 0
            fps_time = 0.0
        
        width, height = glfw.get_framebuffer_size(window)
        
        gl.glFrontFace(gl.GL_CW)
        gl.glEnable(gl.GL_DEPTH_TEST)
        gl.glDepthFunc(gl.GL_LEQUAL)
        gl.glEnable(gl.GL_BLEND)
        gl.glBlendFunc(gl.GL_SRC_ALPHA, gl.GL_ONE_MINUS_SRC_ALPHA)
        gl.glClearColor(0.15, 0.15, 0.15, 1.0)
        gl.glClear(gl.GL_COLOR_BUFFER_BIT | gl.GL_DEPTH_BUFFER_BIT)
        
        model.draw(width, height)
        
        glfw.swap_buffers(window)
        glfw.poll_events()
    
    gl.glFinish()
    dispose()
    glfw.terminate()

if __name__ == "__main__":
    main()
