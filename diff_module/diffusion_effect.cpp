#include "diffusion_effect.h"

#include "core/config/project_settings.h"
#include "core/config/engine.h"
#include "core/input/input.h"
#include "core/input/input_map.h"
#include "core/os/keyboard.h"
#include "core/os/os.h"
#include "core/object/class_db.h"
#include "core/io/image.h"
#include "core/io/file_access.h"
#include "servers/rendering/renderer_rd/storage_rd/render_data_rd.h"
#include "servers/rendering/renderer_rd/storage_rd/render_scene_buffers_rd.h"

static bool g_diffusion_prev_key4_pressed = false;

void DiffusionEffect::_bind_methods() {
	ClassDB::bind_method(D_METHOD("trigger_diffusion"), &DiffusionEffect::trigger_diffusion);
}

DiffusionEffect::DiffusionEffect() {
	set_effect_callback_type(CompositorEffect::EFFECT_CALLBACK_TYPE_POST_OPAQUE);
	set_access_resolved_color(true);
	set_access_resolved_depth(true);

	String method = OS::get_singleton()->get_current_rendering_method();
	bool is_forward_plus = (method == "forward_plus");
	if (is_forward_plus) {
		set_needs_motion_vectors(true);
		set_needs_normal_roughness(true);
	}
}

void DiffusionEffect::_render_callback(int p_effect_callback_type, const RenderData *p_render_data,
    RID &p_color, RID &p_depth, RID &p_motion, RID &p_normal_roughness) {
    
    if (!ProjectSettings::get_singleton()->get_setting("diffusion/enabled", false).operator bool()) {
        // Only log once to avoid spam
        static bool logged_disabled = false;
        if (!logged_disabled) {
            print_line("Diffusion module: disabled in project settings. Enable 'diffusion/enabled' to use.");
            logged_disabled = true;
        }
        return;
    }

    // Check for keypress first before doing any heavy processing
    bool in_editor = Engine::get_singleton()->is_editor_hint();
    if (in_editor) { 
        return; 
    }

    // Use action system instead of direct key check for better reliability
    bool should_generate = should_trigger_diffusion; // Check manual trigger first
    should_trigger_diffusion = false; // Reset manual trigger
    
    if (!should_generate) {
        if (InputMap::get_singleton()->has_action("diffusion_generate")) {
            should_generate = Input::get_singleton()->is_action_just_pressed("diffusion_generate");
        } else {
            // Fallback to direct key check if action doesn't exist
            bool pressed = Input::get_singleton()->is_key_pressed(Key::KEY_4);
            should_generate = pressed && !g_diffusion_prev_key4_pressed;
            g_diffusion_prev_key4_pressed = pressed;
        }
    }
    
    if (!should_generate) {
        return;
    }
    
    // Key pressed, execute diffusion logic
    print_line("Diffusion module: Generation triggered!");

    const RenderDataRD *rd = static_cast<const RenderDataRD *>(p_render_data);
    if (!rd) { 
        ERR_PRINT("Diffusion module: Invalid render data");
        return; 
    }

    Ref<RenderSceneBuffersRD> rb = rd->render_buffers; 
    if (rb.is_null()) { 
        ERR_PRINT("Diffusion module: Invalid render buffers");
        return; 
    }

    // fetch color + depth, if forward+, fetch motion + normal as well
    String method = OS::get_singleton()->get_current_rendering_method();
    bool is_forward_plus = (method == "forward_plus");

    RID color = rb->get_back_buffer_texture();
    RID depth = rb->get_depth_texture();

    RID motion = RID();
    RID normal_roughness = RID();
    if (is_forward_plus) {
        motion = rb->get_velocity_buffer(false);
        if (rb->has_texture(SNAME("forward_clustered"), SNAME("normal_roughness"))) {
            normal_roughness = rb->get_texture(SNAME("forward_clustered"), SNAME("normal_roughness"));
        }
    }

    p_color = color;
    p_depth = depth;
    p_motion = motion;
    p_normal_roughness = normal_roughness;

    // Execute diffusion generation
    Size2i size = rb->get_internal_size();
    float z_near = rd->scene_data->z_near;
    float z_far = rd->scene_data->z_far;
    
    print_line("Diffusion module: Processing depth buffer - Size: " + String::num(size.x) + "x" + String::num(size.y));
    _run_lightning_with_depth(depth, size.x, size.y, z_near, z_far);
}

void DiffusionEffect::_run_lightning_with_depth(const RID &p_depth, int p_width, int p_height, float p_z_near, float p_z_far) {
	if (!p_depth.is_valid() || p_width <= 0 || p_height <= 0) {
		ERR_PRINT("Diffusion module: Invalid depth buffer or dimensions");
		return;
	}
	
	print_line("Diffusion module: Extracting depth data...");
	PackedByteArray raw = RD::get_singleton()->texture_get_data(p_depth, 0);
	if (raw.size() < p_width * p_height * (int)sizeof(float)) {
		ERR_PRINT("Diffusion module: Insufficient depth data size. Expected: " + String::num(p_width * p_height * sizeof(float)) + ", got: " + String::num(raw.size()));
		return;
	}
	
	const float *src = reinterpret_cast<const float *>(raw.ptr());
	PackedByteArray gray;
	gray.resize(p_width * p_height);
	uint8_t *dst = gray.ptrw();
	for (int i = 0; i < p_width * p_height; i++) {
		float z_ndc = src[i];
		float denom = (p_z_far + p_z_near - z_ndc * (p_z_far - p_z_near));
		float z_lin = (2.0f * p_z_near * p_z_far) / (denom == 0.0f ? 1e-6f : denom);
		float nz = (z_lin - p_z_near) / ((p_z_far - p_z_near) == 0.0f ? 1e-6f : (p_z_far - p_z_near));
		nz = CLAMP(nz, 0.0f, 1.0f);
		dst[i] = (uint8_t)(nz * 255.0f);
	}
	
	Ref<Image> img; 
	img.instantiate();
	img->create(p_width, p_height, false, Image::FORMAT_L8, gray);
	
	// Save depth image to the diff_stuff directory where lightning_gen.py expects it
	String diff_dir = ProjectSettings::get_singleton()->globalize_path("res://diff_stuff");
	String out_path = diff_dir.path_join("out_depth.png");
	
	Error save_err = img->save_png(out_path);
	if (save_err != OK) {
		ERR_PRINT("Diffusion module: Failed to save depth image to: " + out_path + " (Error: " + String::num(save_err) + ")");
		return;
	}
	print_line("Diffusion module: Depth image saved to: " + out_path);
	
	// Call the existing lightning_gen.py script
	String script_path = diff_dir.path_join("lightning_gen.py");
	if (!FileAccess::exists(script_path)) {
		ERR_PRINT("Diffusion module: Lightning generation script not found at: " + script_path);
		ERR_PRINT("Diffusion module: Please ensure lightning_gen.py exists in your project's diff_stuff folder");
		return;
	}
	
	Vector<String> args;
	String os_name = OS::get_singleton()->get_name();
	print_line("Diffusion module: Executing lightning generation script on " + os_name + "...");
	
	if (os_name == "Windows") {
		// Use cmd to execute python script in the diff_stuff directory
		String cmd = String("cd /d \"") + diff_dir + String("\" && python \"") + script_path.get_file() + String("\"");
		args.push_back("/C");
		args.push_back(cmd);
		Error exec_err = OS::get_singleton()->execute("cmd", args, nullptr, nullptr, false, false);
		if (exec_err != OK) {
			ERR_PRINT("Diffusion module: Failed to execute lightning generation script (Error: " + String::num(exec_err) + ")");
		} else {
			print_line("Diffusion module: Lightning generation script execution started successfully");
		}
	} else {
		// Use bash for Linux/macOS
		String sh = String("cd \"") + diff_dir + String("\" && python3 \"") + script_path.get_file() + String("\"");
		args.push_back("-lc");
		args.push_back(sh);
		Error exec_err = OS::get_singleton()->execute("/bin/bash", args, nullptr, nullptr, false, false);
		if (exec_err != OK) {
			ERR_PRINT("Diffusion module: Failed to execute lightning generation script (Error: " + String::num(exec_err) + ")");
		} else {
			print_line("Diffusion module: Lightning generation script execution started successfully");
		}
	}
}

void DiffusionEffect::trigger_diffusion() {
	should_trigger_diffusion = true;
	print_line("Diffusion module: Manual trigger requested");
}