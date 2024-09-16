extends MeshInstance3D

var next_shapes = []

func _notification(what):
	if what == NOTIFICATION_WM_CLOSE_REQUEST or what == NOTIFICATION_WM_GO_BACK_REQUEST:
		Controller.client.disconnect()

var chunk_renderer_guard: Mutex = Mutex.new()
func _on_client_chunk_loaded(x, y, geojson_string):
	print("GODOT recieved signal from client that loaded %f %f" % [x, y])
	#if Controller.bounds_set:
	#	return
	if geojson_string != null:
		chunk_renderer_guard.lock()
		Controller.geojson_parse(geojson_string)
		for geometry_dataset in Controller.geometry_dictionary:
			for shape in geometry_dataset:
				if shape.get("geometry").get("type") == "LineString":
					Controller.render_line(shape, mesh, Mesh.PRIMITIVE_LINE_STRIP)
				#elif shape.get("geometry").get("type") == "MultiLineString":
				#	Controller.render_multiline(shape, mesh, Mesh.PRIMITIVE_LINE_STRIP)
				elif shape.get("geometry").get("type") == "Polygon":
					Controller.render_polygon(shape, mesh, Mesh.PRIMITIVE_TRIANGLES, Mesh.PRIMITIVE_TRIANGLE_STRIP, 0.01)
				elif shape.get("geometry").get("type") == "MultiPolygon":
					Controller.render_multipolygon(shape, mesh, Mesh.PRIMITIVE_LINES, Mesh.PRIMITIVE_LINE_STRIP, Controller.color_materials[0], 0.014)
				elif shape.get("geometry").get("type") == "Point":
					Controller.render_point(shape, mesh, Mesh.PRIMITIVE_TRIANGLES)
		chunk_renderer_guard.unlock()

# Called when the node enters the scene tree for the first time.
func _ready():
	
	Controller.client.chunk_loaded.connect(_on_client_chunk_loaded)
	var res = Controller.client.connect_to_server("localhost", 12345)
	print("connection result: %d" % res)
	
	Controller.client.move_chunk_center(11.545, 48.145)
	
	#var test_geojson_res = Controller.client.get_chunk_info(11.545, 48.145)
	#_on_client_chunk_loaded(11.545, 48.145, test_geojson_res)
	#print(res3)
	

# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(delta):
	pass
