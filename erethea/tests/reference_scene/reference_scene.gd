extends Node3D
## Erethea reference scene: a village, a forest and a character, by day and by
## night. Everything is built from code, so the scene needs no imported assets
## and looks the same in Erethea and in stock Godot. It is used to compare the
## look and the speed of each change, see "Cena de referência" in
## erethea/README.md.
##
## Interactive: keys 1, 2 and 3 change the view, N switches day and night, and
## the window title shows the frame rate.
##
## Capture: run with a fixed frame rate and user arguments after "--":
##     erethea --path erethea/tests/reference_scene --fixed-fps 30 -- --capture=<folder>
## Give <folder> as an absolute path: a relative one is inside the project. It saves <time>_<view>.png for each time of day and view, and results.json
## with the frame times. Optional arguments: --warmup=<frames> before each
## capture (default 20) and --frames=<frames> measured after it (default 20).
##
## In Erethea, --quality=<low|balanced|cinematic|automatic> applies a quality
## preset (EretheaQuality) first, and --post=<effects> turns on post-processing:
## "all", or a comma-separated list of lut (luts/cinematic.cube as color
## correction, which needs the project imported once with --headless --import),
## flare, vignette, aberration and grain (EretheaPostEffect), and motion
## (EretheaMotionBlur: the camera pans and the character turns, and both are
## back in their usual place, at full speed, on the captured frame). Stock
## Godot ignores both with a warning.
##
## --upscaler=<fsr2|amd_fsr|xess> renders at 67% of the resolution and upscales
## with Godot's FSR 2.2 or, in Erethea, with AMD FSR 3.1/4 or XeSS (Windows,
## Vulkan or Direct3D 12); both fall back to FSR 2.2 elsewhere. On Vulkan, XeSS
## also needs the project setting before Erethea starts: put
## "[rendering]\nerethea/temporal_upscaler=2" in an override.cfg next to
## project.godot. --upscale-scale=<0.25..1> changes the 67%.

const TIMES: Array[String] = ["dia", "noite"]
const VIEWS := {
	"vila": [Vector3(8.0, 4.5, 9.5), Vector3(-4.0, 1.5, -5.0)],
	"floresta": [Vector3(-26.0, 1.7, -22.0), Vector3(-42.0, 4.0, -26.0)],
	"personagem": [Vector3(3.1, 1.65, 6.4), Vector3(1.5, 1.3, 3.0)],
}
const VIEW_KEYS := {KEY_1: "vila", KEY_2: "floresta", KEY_3: "personagem"}
# The values of EretheaQuality.Preset, written out so stock Godot can parse the
# script.
const QUALITY_PRESETS := {"low": 1, "balanced": 2, "cinematic": 3, "automatic": 4}
const POST_EFFECTS: Array[String] = ["lut", "flare", "vignette", "aberration", "grain", "motion"]
# Swing of the camera pan and of the character's turn, in degrees, for motion blur.
const MOTION_PAN := 12.0
const MOTION_TURN := 60.0
const LUT_PATH := "res://luts/cinematic.cube"
# The values of the Erethea setting rendering/erethea/temporal_upscaler.
const UPSCALERS := {"fsr2": 0, "amd_fsr": 1, "xess": 2}
const UPSCALER_SETTING := "rendering/erethea/temporal_upscaler"
const CAPTURE_SIZE := Vector2i(1280, 720)
const SEED := 2026

var rng := RandomNumberGenerator.new()
var environment := Environment.new()
var sky_material := ProceduralSkyMaterial.new()
var sun := DirectionalLight3D.new()
var camera := Camera3D.new()
var lantern_lights: Array[OmniLight3D] = []
var window_material := StandardMaterial3D.new()
var lantern_material := StandardMaterial3D.new()
var current_time := "dia"
var current_view := "vila"
var character := Node3D.new()
var camera_transform := Transform3D()
var character_transform := Transform3D()
# Motion for the motion blur: frames since the view changed, over one swing.
var motion_enabled := false
var motion_frame := 0
var motion_period := 40


func _ready() -> void:
	rng.seed = SEED
	_build_environment()
	_build_ground()
	_build_village()
	_build_forest()
	_build_character()
	add_child(camera)
	camera.current = true
	camera.fov = 60.0
	camera.far = 400.0
	set_time("dia")
	set_view("vila")

	var arguments := _get_user_arguments()
	if arguments.has("quality"):
		_apply_quality(arguments["quality"])
	if arguments.has("post"):
		_apply_post(arguments["post"])
	if arguments.has("upscaler"):
		_apply_upscaler(arguments["upscaler"], float(arguments.get("upscale-scale", "0.67")))
	if arguments.has("capture"):
		await _capture(arguments["capture"], int(arguments.get("warmup", "20")), int(arguments.get("frames", "20")))
		get_tree().quit()


func _process(_delta: float) -> void:
	get_window().title = "Erethea Reference Scene: %s, %s, %d FPS" % [current_view, current_time, Engine.get_frames_per_second()]
	if motion_enabled:
		# A full swing per period: back in place, at full speed, every
		# motion_period frames after the view changed.
		motion_frame += 1
		var swing := sin(TAU * motion_frame / motion_period)
		camera.transform = camera_transform.rotated_local(Vector3.UP, deg_to_rad(MOTION_PAN) * swing)
		character.transform = character_transform.rotated_local(Vector3.UP, deg_to_rad(MOTION_TURN) * swing)


func _unhandled_key_input(event: InputEvent) -> void:
	var key := event as InputEventKey
	if key == null or not key.pressed or key.echo:
		return
	if VIEW_KEYS.has(key.keycode):
		set_view(VIEW_KEYS[key.keycode])
	elif key.keycode == KEY_N:
		set_time("noite" if current_time == "dia" else "dia")


func set_view(view: String) -> void:
	current_view = view
	var position_and_target: Array = VIEWS[view]
	camera.look_at_from_position(position_and_target[0], position_and_target[1])
	camera_transform = camera.transform
	character.transform = character_transform
	motion_frame = 0


func set_time(time: String) -> void:
	current_time = time
	var night := time == "noite"
	if night:
		sky_material.sky_top_color = Color(0.01, 0.015, 0.04)
		sky_material.sky_horizon_color = Color(0.04, 0.05, 0.09)
		sky_material.ground_horizon_color = Color(0.03, 0.035, 0.05)
		sky_material.ground_bottom_color = Color(0.01, 0.01, 0.015)
		sky_material.sky_energy_multiplier = 0.6
		sun.rotation_degrees = Vector3(-35.0, 120.0, 0.0)
		sun.light_color = Color(0.55, 0.65, 1.0)
		sun.light_energy = 0.3
		environment.ambient_light_energy = 0.7
		environment.fog_light_color = Color(0.05, 0.06, 0.1)
		environment.fog_density = 0.006
		environment.volumetric_fog_enabled = true
		window_material.emission_energy_multiplier = 4.0
		lantern_material.emission_energy_multiplier = 8.0
	else:
		sky_material.sky_top_color = Color(0.28, 0.48, 0.82)
		sky_material.sky_horizon_color = Color(0.68, 0.76, 0.86)
		sky_material.ground_horizon_color = Color(0.68, 0.76, 0.86)
		sky_material.ground_bottom_color = Color(0.25, 0.22, 0.2)
		sky_material.sky_energy_multiplier = 1.0
		sun.rotation_degrees = Vector3(-42.0, -35.0, 0.0)
		sun.light_color = Color(1.0, 0.95, 0.86)
		sun.light_energy = 1.4
		environment.ambient_light_energy = 1.0
		environment.fog_light_color = Color(0.62, 0.7, 0.8)
		environment.fog_density = 0.0035
		environment.volumetric_fog_enabled = false
		window_material.emission_energy_multiplier = 0.0
		lantern_material.emission_energy_multiplier = 0.0
	for light in lantern_lights:
		light.visible = night


func _apply_quality(preset: String) -> void:
	if not QUALITY_PRESETS.has(preset):
		push_error("Unknown quality preset '%s'. Use one of %s." % [preset, ", ".join(PackedStringArray(QUALITY_PRESETS.keys()))])
		return
	if not Engine.has_singleton("EretheaQuality"):
		push_warning("This engine has no quality presets, ignoring --quality=%s." % preset)
		return
	var quality := Engine.get_singleton("EretheaQuality")
	quality.call("apply_preset", QUALITY_PRESETS[preset])
	print("Quality preset %s applied: %s" % [preset, quality.call("get_preset_settings", quality.call("get_active_preset"))])


func _apply_post(effects_argument: String) -> void:
	var effects: Array[String] = []
	if effects_argument == "all":
		effects = POST_EFFECTS
	else:
		effects.assign(effects_argument.split(","))
	for effect_name in effects:
		if effect_name not in POST_EFFECTS:
			push_error("Unknown post effect '%s'. Use all or some of %s." % [effect_name, ",".join(POST_EFFECTS)])
			return

	if "lut" in effects:
		if ResourceLoader.exists(LUT_PATH, "Texture3D"):
			environment.adjustment_enabled = true
			environment.adjustment_color_correction = load(LUT_PATH)
		else:
			push_warning("%s isn't imported (stock Godot can't import .cube, and Erethea needs --headless --import once), skipping the LUT." % LUT_PATH)

	if effects.any(func(effect_name: String) -> bool: return effect_name != "lut"):
		if not ClassDB.class_exists("EretheaPostEffect"):
			push_warning("This engine has no EretheaPostEffect, ignoring --post=%s." % effects_argument)
			return
		var effect: CompositorEffect = ClassDB.instantiate("EretheaPostEffect")
		if "flare" in effects:
			effect.set("lens_flare_intensity", 0.5)
		if "vignette" in effects:
			effect.set("vignette_intensity", 0.4)
		if "aberration" in effects:
			effect.set("chromatic_aberration", 0.3)
		if "grain" in effects:
			effect.set("film_grain_intensity", 0.2)
		var compositor_effects: Array[CompositorEffect] = []
		if "motion" in effects:
			# Before the other effects, so the grain isn't blurred.
			if ClassDB.class_exists("EretheaMotionBlur"):
				compositor_effects.append(ClassDB.instantiate("EretheaMotionBlur"))
				motion_enabled = true
			else:
				push_warning("This engine has no EretheaMotionBlur, ignoring motion.")
		compositor_effects.append(effect)
		var compositor := Compositor.new()
		compositor.compositor_effects = compositor_effects
		camera.compositor = compositor
	print("Post effects: %s" % ", ".join(effects))


func _apply_upscaler(upscaler: String, scale: float) -> void:
	if not UPSCALERS.has(upscaler):
		push_error("Unknown upscaler '%s'. Use one of %s." % [upscaler, ", ".join(PackedStringArray(UPSCALERS.keys()))])
		return
	get_viewport().scaling_3d_mode = Viewport.SCALING_3D_MODE_FSR2
	get_viewport().scaling_3d_scale = clampf(scale, 0.25, 1.0)
	if ProjectSettings.has_setting(UPSCALER_SETTING):
		ProjectSettings.set_setting(UPSCALER_SETTING, UPSCALERS[upscaler])
	elif upscaler != "fsr2":
		push_warning("This engine has no %s, upscaling with FSR 2.2." % UPSCALER_SETTING)
	print("Upscaler: %s at %d%%" % [upscaler, roundi(get_viewport().scaling_3d_scale * 100.0)])


func _get_user_arguments() -> Dictionary:
	var arguments := {}
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with("--") and argument.contains("="):
			var parts := argument.trim_prefix("--").split("=", true, 1)
			arguments[parts[0]] = parts[1]
	return arguments


func _capture(folder: String, warmup: int, frames: int) -> void:
	# A relative folder is inside the project, the working directory when the
	# engine runs with --path.
	if folder.contains("://"):
		folder = ProjectSettings.globalize_path(folder)
	elif folder.is_relative_path():
		folder = ProjectSettings.globalize_path("res://").path_join(folder)
	DirAccess.make_dir_recursive_absolute(folder)
	get_window().size = CAPTURE_SIZE
	motion_period = maxi(warmup + frames, 1)
	var viewport_rid := get_viewport().get_viewport_rid()
	RenderingServer.viewport_set_measure_render_time(viewport_rid, true)

	var results := {
		"engine": Engine.get_version_info(),
		"adapter": RenderingServer.get_video_adapter_name(),
		"vendor": RenderingServer.get_video_adapter_vendor(),
		"api": RenderingServer.get_video_adapter_api_version(),
		"size": [CAPTURE_SIZE.x, CAPTURE_SIZE.y],
		"quality": _get_user_arguments().get("quality", "default"),
		"post": _get_user_arguments().get("post", "none"),
		"upscaler": _get_user_arguments().get("upscaler", "none"),
		"captures": [],
	}
	print("Reference scene on %s (%s)" % [results["adapter"], results["api"]])

	for time in TIMES:
		set_time(time)
		for view: String in VIEWS:
			set_view(view)
			for i in warmup:
				await RenderingServer.frame_post_draw

			var gpu_ms := 0.0
			var cpu_ms := 0.0
			var start := Time.get_ticks_usec()
			for i in frames:
				await RenderingServer.frame_post_draw
				gpu_ms += RenderingServer.viewport_get_measured_render_time_gpu(viewport_rid)
				cpu_ms += RenderingServer.viewport_get_measured_render_time_cpu(viewport_rid)
			var frame_ms := (Time.get_ticks_usec() - start) / 1000.0 / maxi(frames, 1)

			var image := get_viewport().get_texture().get_image()
			var file_name := "%s_%s.png" % [time, view]
			if image.get_size() != CAPTURE_SIZE:
				push_error("Captured %s at %s instead of %s." % [file_name, image.get_size(), CAPTURE_SIZE])
			image.save_png(folder.path_join(file_name))
			var capture := {
				"name": file_name.get_basename(),
				"frame_ms": snappedf(frame_ms, 0.01),
				"gpu_ms": snappedf(gpu_ms / maxi(frames, 1), 0.01),
				"cpu_ms": snappedf(cpu_ms / maxi(frames, 1), 0.01),
			}
			results["captures"].append(capture)
			print("Captured %s: %.2f ms per frame (GPU %.2f ms)" % [file_name, capture["frame_ms"], capture["gpu_ms"]])

	var file := FileAccess.open(folder.path_join("results.json"), FileAccess.WRITE)
	file.store_string(JSON.stringify(results, "\t"))
	file.close()
	print("Captures saved in %s" % folder)


# Building the scene.


func _material(color: Color, roughness := 0.85) -> StandardMaterial3D:
	var material := StandardMaterial3D.new()
	material.albedo_color = color
	material.roughness = roughness
	return material


func _add_mesh(mesh: Mesh, material: Material, transform: Transform3D, parent: Node3D = self) -> MeshInstance3D:
	var instance := MeshInstance3D.new()
	instance.mesh = mesh
	instance.material_override = material
	instance.transform = transform
	parent.add_child(instance)
	return instance


func _box(size: Vector3) -> BoxMesh:
	var mesh := BoxMesh.new()
	mesh.size = size
	return mesh


func _cylinder(top_radius: float, bottom_radius: float, height: float, segments := 16) -> CylinderMesh:
	var mesh := CylinderMesh.new()
	mesh.top_radius = top_radius
	mesh.bottom_radius = bottom_radius
	mesh.height = height
	mesh.radial_segments = segments
	return mesh


func _sphere(radius: float, segments := 24) -> SphereMesh:
	var mesh := SphereMesh.new()
	mesh.radius = radius
	mesh.height = radius * 2.0
	mesh.radial_segments = segments
	mesh.rings = segments >> 1
	return mesh


func _capsule(radius: float, height: float) -> CapsuleMesh:
	var mesh := CapsuleMesh.new()
	mesh.radius = radius
	mesh.height = height
	return mesh


func _noise_texture(noise_seed: int, frequency: float, low: Color, high: Color, normal_strength := 0.0) -> ImageTexture:
	var noise := FastNoiseLite.new()
	noise.seed = noise_seed
	noise.frequency = frequency
	noise.fractal_octaves = 5
	var image := noise.get_seamless_image(512, 512)
	if normal_strength > 0.0:
		image.bump_map_to_normal_map(normal_strength)
	else:
		image.convert(Image.FORMAT_RGB8)
		for y in image.get_height():
			for x in image.get_width():
				image.set_pixel(x, y, low.lerp(high, image.get_pixel(x, y).r))
	image.generate_mipmaps()
	return ImageTexture.create_from_image(image)


func _build_environment() -> void:
	var sky := Sky.new()
	sky.sky_material = sky_material
	environment.background_mode = Environment.BG_SKY
	environment.sky = sky
	environment.ambient_light_source = Environment.AMBIENT_SOURCE_SKY
	environment.reflected_light_source = Environment.REFLECTION_SOURCE_SKY
	environment.tonemap_mode = Environment.TONE_MAPPER_AGX
	environment.ssao_enabled = true
	environment.glow_enabled = true
	environment.fog_enabled = true
	environment.fog_sky_affect = 0.3
	environment.volumetric_fog_density = 0.02
	environment.volumetric_fog_albedo = Color(0.8, 0.8, 0.9)
	var world_environment := WorldEnvironment.new()
	world_environment.environment = environment
	add_child(world_environment)

	sun.shadow_enabled = true
	sun.directional_shadow_max_distance = 120.0
	add_child(sun)


func _build_ground() -> void:
	var grass := _material(Color.WHITE, 0.95)
	grass.albedo_texture = _noise_texture(1, 0.02, Color(0.16, 0.26, 0.08), Color(0.34, 0.45, 0.16))
	grass.normal_enabled = true
	grass.normal_texture = _noise_texture(2, 0.05, Color.BLACK, Color.WHITE, 4.0)
	grass.uv1_scale = Vector3(30.0, 30.0, 1.0)
	var ground := PlaneMesh.new()
	ground.size = Vector2(260.0, 260.0)
	_add_mesh(ground, grass, Transform3D.IDENTITY)

	var cobbles := _material(Color.WHITE, 0.9)
	cobbles.albedo_texture = _noise_texture(3, 0.08, Color(0.3, 0.28, 0.26), Color(0.55, 0.52, 0.48))
	cobbles.normal_enabled = true
	cobbles.normal_texture = _noise_texture(4, 0.12, Color.BLACK, Color.WHITE, 6.0)
	cobbles.uv1_scale = Vector3(4.0, 4.0, 1.0)
	_add_mesh(_cylinder(10.0, 10.0, 0.06, 48), cobbles, Transform3D(Basis.IDENTITY, Vector3(0.0, 0.03, 0.0)))


func _build_village() -> void:
	var plaster: Array[StandardMaterial3D] = [
		_material(Color(0.86, 0.82, 0.72)),
		_material(Color(0.78, 0.7, 0.58)),
		_material(Color(0.9, 0.88, 0.84)),
	]
	var roofs: Array[StandardMaterial3D] = [_material(Color(0.45, 0.16, 0.1), 0.7), _material(Color(0.25, 0.2, 0.18), 0.7)]
	var wood := _material(Color(0.24, 0.15, 0.09))
	var stone := _material(Color(0.45, 0.44, 0.42), 0.95)

	window_material.albedo_color = Color(0.1, 0.12, 0.16)
	window_material.roughness = 0.15
	window_material.emission_enabled = true
	window_material.emission = Color(1.0, 0.66, 0.32)
	lantern_material.albedo_color = Color(1.0, 0.9, 0.7)
	lantern_material.emission_enabled = true
	lantern_material.emission = Color(1.0, 0.7, 0.35)

	# Houses around the square, facing its center.
	var house_count := 8
	for i in house_count:
		var angle := TAU * i / house_count + 0.3
		var distance := 15.0 + rng.randf_range(-1.0, 1.5)
		var size := Vector3(rng.randf_range(5.0, 7.0), rng.randf_range(3.2, 4.2), rng.randf_range(4.5, 6.0))
		var spot := Vector3(sin(angle), 0.0, cos(angle)) * distance
		var house := Node3D.new()
		add_child(house)
		house.look_at_from_position(spot, Vector3.ZERO)
		house.rotate_object_local(Vector3.UP, PI)
		_add_house(house, size, plaster[i % plaster.size()], roofs[i % roofs.size()], wood, stone)

	# A well in the middle of the square.
	_add_mesh(_cylinder(1.1, 1.2, 0.9, 24), stone, Transform3D(Basis.IDENTITY, Vector3(0.0, 0.45, 0.0)))
	_add_mesh(_cylinder(0.85, 0.85, 0.05, 24), _material(Color(0.03, 0.05, 0.06), 0.05), Transform3D(Basis.IDENTITY, Vector3(0.0, 0.88, 0.0)))
	for side in [-1.0, 1.0]:
		_add_mesh(_box(Vector3(0.15, 2.2, 0.15)), wood, Transform3D(Basis.IDENTITY, Vector3(side * 1.0, 1.1, 0.0)))
	var well_roof := PrismMesh.new()
	well_roof.size = Vector3(2.8, 0.8, 1.6)
	_add_mesh(well_roof, roofs[0], Transform3D(Basis.IDENTITY, Vector3(0.0, 2.6, 0.0)))

	# Barrels and crates.
	for i in 10:
		var angle := rng.randf() * TAU
		var spot := Vector3(sin(angle), 0.0, cos(angle)) * rng.randf_range(6.0, 9.0)
		if rng.randf() < 0.5:
			_add_mesh(_cylinder(0.35, 0.35, 0.9, 16), wood, Transform3D(Basis.IDENTITY, spot + Vector3(0.0, 0.45, 0.0)))
		else:
			var spin := Basis(Vector3.UP, rng.randf() * TAU)
			_add_mesh(_box(Vector3(0.8, 0.8, 0.8)), wood, Transform3D(spin, spot + Vector3(0.0, 0.4, 0.0)))


func _add_house(house: Node3D, size: Vector3, plaster: Material, roof: Material, wood: Material, stone: Material) -> void:
	# The house faces +Z of its own space.
	_add_mesh(_box(Vector3(size.x + 0.2, 0.4, size.z + 0.2)), stone, Transform3D(Basis.IDENTITY, Vector3(0.0, 0.2, 0.0)), house)
	_add_mesh(_box(size), plaster, Transform3D(Basis.IDENTITY, Vector3(0.0, size.y * 0.5, 0.0)), house)
	for x in [-0.5, 0.5]:
		for z in [-0.5, 0.5]:
			var corner := Vector3(size.x * x, size.y * 0.5, size.z * z)
			_add_mesh(_box(Vector3(0.25, size.y, 0.25)), wood, Transform3D(Basis.IDENTITY, corner), house)
	_add_mesh(_box(Vector3(size.x + 0.1, 0.2, size.z + 0.1)), wood, Transform3D(Basis.IDENTITY, Vector3(0.0, size.y, 0.0)), house)

	var roof_height := size.x * 0.45
	var roof_mesh := PrismMesh.new()
	roof_mesh.size = Vector3(size.x + 0.8, roof_height, size.z + 0.8)
	_add_mesh(roof_mesh, roof, Transform3D(Basis.IDENTITY, Vector3(0.0, size.y + roof_height * 0.5, 0.0)), house)
	_add_mesh(_box(Vector3(0.6, 1.6, 0.6)), stone, Transform3D(Basis.IDENTITY, Vector3(size.x * 0.25, size.y + roof_height * 0.6, -size.z * 0.2)), house)

	var front := size.z * 0.5 + 0.04
	_add_mesh(_box(Vector3(1.1, 2.1, 0.1)), wood, Transform3D(Basis.IDENTITY, Vector3(0.0, 1.05, front)), house)
	for x in [-0.32, 0.32]:
		_add_mesh(_box(Vector3(0.9, 0.9, 0.08)), window_material, Transform3D(Basis.IDENTITY, Vector3(size.x * x, size.y * 0.6, front)), house)

	# A lantern next to the door, lit at night.
	var lantern_position := Vector3(1.2, 0.0, front + 0.8)
	_add_mesh(_cylinder(0.05, 0.06, 2.4, 8), wood, Transform3D(Basis.IDENTITY, lantern_position + Vector3(0.0, 1.2, 0.0)), house)
	_add_mesh(_sphere(0.14, 12), lantern_material, Transform3D(Basis.IDENTITY, lantern_position + Vector3(0.0, 2.45, 0.0)), house)
	var light := OmniLight3D.new()
	light.light_color = Color(1.0, 0.7, 0.4)
	light.light_energy = 2.5
	light.omni_range = 9.0
	light.shadow_enabled = true
	light.position = lantern_position + Vector3(0.0, 2.2, 0.25)
	house.add_child(light)
	lantern_lights.append(light)


func _build_forest() -> void:
	var trunk_mesh := _cylinder(0.12, 0.2, 2.4, 10)
	var pine_low := _cylinder(0.0, 1.7, 3.2, 12)
	var pine_high := _cylinder(0.0, 1.2, 2.6, 12)
	var crown := _sphere(1.6, 16)
	var transforms := {"trunk": [], "pine_low": [], "pine_high": [], "crown": []}
	var colors := {"trunk": [], "pine_low": [], "pine_high": [], "crown": []}

	var placed := 0
	while placed < 700:
		var angle := rng.randf() * TAU
		var distance := 23.0 + 55.0 * sqrt(rng.randf())
		var spot := Vector3(sin(angle), 0.0, cos(angle)) * distance
		var size_scale := rng.randf_range(0.8, 1.5)
		var spin := Basis(Vector3.UP, rng.randf() * TAU).scaled(Vector3.ONE * size_scale)
		var tint := rng.randf_range(0.75, 1.15)
		transforms["trunk"].append(Transform3D(spin, spot + Vector3(0.0, 1.2 * size_scale, 0.0)))
		colors["trunk"].append(Color(0.3, 0.2, 0.13) * tint)
		if rng.randf() < 0.6:
			transforms["pine_low"].append(Transform3D(spin, spot + Vector3(0.0, 3.4 * size_scale, 0.0)))
			transforms["pine_high"].append(Transform3D(spin, spot + Vector3(0.0, 5.0 * size_scale, 0.0)))
			colors["pine_low"].append(Color(0.1, 0.24, 0.12) * tint)
			colors["pine_high"].append(Color(0.12, 0.28, 0.13) * tint)
		else:
			transforms["crown"].append(Transform3D(spin, spot + Vector3(0.0, 3.6 * size_scale, 0.0)))
			colors["crown"].append(Color(0.24, 0.38, 0.12) * tint)
		placed += 1

	var leaves := _material(Color.WHITE, 0.9)
	leaves.vertex_color_use_as_albedo = true
	var bark := _material(Color.WHITE, 0.95)
	bark.vertex_color_use_as_albedo = true
	_add_multimesh(trunk_mesh, bark, transforms["trunk"], colors["trunk"])
	_add_multimesh(pine_low, leaves, transforms["pine_low"], colors["pine_low"])
	_add_multimesh(pine_high, leaves, transforms["pine_high"], colors["pine_high"])
	_add_multimesh(crown, leaves, transforms["crown"], colors["crown"])

	# Rocks.
	var rock := _material(Color(0.4, 0.4, 0.38), 0.95)
	for i in 30:
		var angle := rng.randf() * TAU
		var spot := Vector3(sin(angle), 0.0, cos(angle)) * rng.randf_range(12.0, 60.0)
		var size_scale := Vector3(rng.randf_range(0.6, 1.6), rng.randf_range(0.4, 0.9), rng.randf_range(0.6, 1.6))
		_add_mesh(_sphere(0.7, 8), rock, Transform3D(Basis(Vector3.UP, rng.randf() * TAU).scaled(size_scale), spot))


func _add_multimesh(mesh: Mesh, material: Material, transforms: Array, colors: Array) -> void:
	var multimesh := MultiMesh.new()
	multimesh.transform_format = MultiMesh.TRANSFORM_3D
	multimesh.use_colors = true
	multimesh.mesh = mesh
	multimesh.instance_count = transforms.size()
	for i in transforms.size():
		multimesh.set_instance_transform(i, transforms[i])
		multimesh.set_instance_color(i, colors[i])
	var instance := MultiMeshInstance3D.new()
	instance.multimesh = multimesh
	instance.material_override = material
	add_child(instance)


func _build_character() -> void:
	var skin := _material(Color(0.78, 0.56, 0.44), 0.6)
	skin.subsurf_scatter_enabled = true
	skin.subsurf_scatter_strength = 0.4
	var tunic := _material(Color(0.55, 0.1, 0.08), 0.8)
	var trousers := _material(Color(0.16, 0.16, 0.2), 0.85)
	var leather := _material(Color(0.22, 0.13, 0.07), 0.6)
	var hair := _material(Color(0.12, 0.07, 0.04), 0.5)
	var eyes := _material(Color(0.03, 0.03, 0.03), 0.2)
	var wood := _material(Color(0.35, 0.24, 0.14), 0.7)

	character.position = Vector3(1.5, 0.0, 3.0)
	character.rotation_degrees.y = 25.0
	character_transform = character.transform
	add_child(character)
	for side in [-1.0, 1.0]:
		_add_mesh(_capsule(0.11, 0.9), trousers, Transform3D(Basis.IDENTITY, Vector3(side * 0.12, 0.5, 0.0)), character)
		_add_mesh(_box(Vector3(0.18, 0.12, 0.3)), leather, Transform3D(Basis.IDENTITY, Vector3(side * 0.12, 0.06, 0.05)), character)
		var arm := Basis(Vector3.BACK, side * 0.12)
		_add_mesh(_capsule(0.075, 0.68), tunic, Transform3D(arm, Vector3(side * 0.33, 1.17, 0.0)), character)
		_add_mesh(_sphere(0.07, 12), skin, Transform3D(Basis.IDENTITY, Vector3(side * 0.37, 0.82, 0.0)), character)
		_add_mesh(_sphere(0.022, 8), eyes, Transform3D(Basis.IDENTITY, Vector3(side * 0.055, 1.75, 0.14)), character)
	_add_mesh(_capsule(0.23, 0.78), tunic, Transform3D(Basis.IDENTITY, Vector3(0.0, 1.2, 0.0)), character)
	_add_mesh(_cylinder(0.235, 0.235, 0.07, 24), leather, Transform3D(Basis.IDENTITY, Vector3(0.0, 1.0, 0.0)), character)
	_add_mesh(_cylinder(0.06, 0.07, 0.12, 12), skin, Transform3D(Basis.IDENTITY, Vector3(0.0, 1.58, 0.0)), character)
	_add_mesh(_sphere(0.15), skin, Transform3D(Basis.IDENTITY.scaled(Vector3(0.95, 1.1, 1.0)), Vector3(0.0, 1.74, 0.0)), character)
	_add_mesh(_sphere(0.155), hair, Transform3D(Basis.IDENTITY.scaled(Vector3(1.0, 0.85, 1.0)), Vector3(0.0, 1.81, -0.025)), character)
	_add_mesh(_cylinder(0.025, 0.03, 1.9, 8), wood, Transform3D(Basis.IDENTITY, Vector3(0.45, 0.95, 0.08)), character)
