extends Node

const INCLUDED_TYPES = ["parking","apartments","yes","retail","office","public", "garden","park","school","construction"]
const GEOMETRY_SCALE = 60000
const SCALE_SPAWN_TO_LOCATION = true
const SCALE_SPEED_TO_LOCATION = true
const GEOMETRY_HEIGHT = 1000

var bias_speed = 0.1
var bias_x = 0
var bias_z = 0
var geometry_dictionary = []
var bounds_set = false

var collider
var client = GDClient.new()

const color_materials : Array = [
	preload("res://materials/blue.tres"),
	preload("res://materials/red.tres"),
	preload("res://materials/green.tres"),
	preload("res://materials/black.tres"),
	preload("res://materials/orange.tres"),
	preload("res://materials/grey.tres")
	]

func geojson_parse(geojson_string):
	#create new json reader
	var json = JSON.new()
	#load geojson as string and parse
	json.parse(geojson_string)
	geometry_dictionary = []
	if not bounds_set:
		bias_x = (json.data[0].get("minx") + json.data[0].get("maxx")) / 2
		bias_z = (json.data[0].get("miny") + json.data[0].get("maxy")) / 2

		bounds_set = true
	
	for e in json.data.slice(1):
		geometry_dictionary.append(e.get("features"))

func render_polygon(polygon, mesh, top_render_mode, side_render_mode, height):
	var polygon_color = color_materials[randi() % 3]
	mesh.surface_begin(side_render_mode, polygon_color)
	for i in polygon.get("geometry").get("coordinates")[0]:
		mesh.surface_add_vertex(scale_to_world_space(i[0],height,i[1]))
		mesh.surface_add_vertex(scale_to_world_space(i[0],0,i[1]))
	mesh.surface_end()
	
	mesh.surface_begin(side_render_mode, polygon_color)
	for i in polygon.get("geometry").get("coordinates")[0]:
		mesh.surface_add_vertex(scale_to_world_space(i[0],0,i[1]))
		mesh.surface_add_vertex(scale_to_world_space(i[0],height,i[1]))
	mesh.surface_end()
	
	polygon.get("geometry").get("coordinates")[0].reverse()
	var top_geometry = polygon.get("geometry").get("coordinates")[0]
	var top_vector2 = PackedVector2Array()
	for i in top_geometry:
		top_vector2.append(Vector2(i[0],i[1]))
	var fixed = Geometry2D.triangulate_polygon (top_vector2)
	mesh.surface_begin(top_render_mode, polygon_color)
	for i in fixed:
		mesh.surface_add_vertex(scale_to_world_space(top_vector2[i][0], height,top_vector2[i][1]))
	mesh.surface_end()
	fixed.reverse()
	mesh.surface_begin(top_render_mode, polygon_color)
	for i in fixed:
		mesh.surface_add_vertex(scale_to_world_space(top_vector2[i][0], height,top_vector2[i][1]))
	mesh.surface_end()

func render_multipolygon(polygon, mesh, top_render_mode, render_mode, polygon_color, height):
	for i in polygon.get("geometry").get("coordinates")[0]:
		mesh.surface_begin(render_mode, polygon_color)
		i.reverse()
		for j in i:
			mesh.surface_add_vertex(scale_to_world_space(j[0],height,j[1]))
			mesh.surface_add_vertex(scale_to_world_space(j[0],0,j[1]))
		mesh.surface_end()
		mesh.surface_begin(render_mode, polygon_color)
		var points_for_collider = []
		for j in i:
			mesh.surface_add_vertex(scale_to_world_space(j[0],0,j[1]))
			mesh.surface_add_vertex(scale_to_world_space(j[0],height,j[1]))
		mesh.surface_end()
		collider = ConvexPolygonShape3D.new()
		collider.points = points_for_collider


	for i in polygon.get("geometry").get("coordinates")[0]:
		i.reverse()
		var top_geometry = i
		var top_vector2 = PackedVector2Array()
		for j in top_geometry:
			top_vector2.append(Vector2(j[0],j[1]))
		var fixed = Geometry2D.triangulate_polygon (top_vector2)
		mesh.surface_begin(top_render_mode, polygon_color)
		for j in fixed:
			mesh.surface_add_vertex(scale_to_world_space(top_vector2[j][0], height, top_vector2[j][1]))
		mesh.surface_end()
		fixed.reverse()
		mesh.surface_begin(top_render_mode, polygon_color)
		for j in fixed:
			mesh.surface_add_vertex(scale_to_world_space(top_vector2[j][0], height, top_vector2[j][1]))
		mesh.surface_end()



func render_line(line, mesh, render_mode):
	mesh.surface_begin(render_mode, color_materials[3])
	for i in line.get("geometry").get("coordinates"):
		mesh.surface_add_vertex(scale_to_world_space(i[0],0,i[1]))
	mesh.surface_end()

func render_multiline(line, mesh, render_mode):
	for i in line.get("geometry").get("coordinates"):
		mesh.surface_begin(render_mode, color_materials[3])
		for j in i:
			mesh.surface_add_vertex(scale_to_world_space(i[0],0,i[1]))
		mesh.surface_end()

func render_point(point, mesh, render_mode):
	mesh.surface_begin(render_mode)
	point=point.get("geometry").get("coordinates")
	#print(point)
	point[1] = -1*point[1]
	#makes triangle at point
	mesh.surface_add_vertex(Vector3(point[0]*GEOMETRY_SCALE, 0, point[1]*GEOMETRY_SCALE+bias_x*GEOMETRY_SCALE*0.01))
	mesh.surface_add_vertex(Vector3(point[0]*GEOMETRY_SCALE-bias_x*GEOMETRY_SCALE*0.01, 0, point[1]*GEOMETRY_SCALE))
	mesh.surface_add_vertex(Vector3(point[0]*GEOMETRY_SCALE+bias_x*GEOMETRY_SCALE*0.01, 0, point[1]*GEOMETRY_SCALE))
	mesh.surface_end()

func process_player_position(pos):
	if bounds_set:
		var x = (pos.x + bias_x) / GEOMETRY_SCALE
		var y = -1 * (pos.z + bias_z) / GEOMETRY_SCALE
		#print("current pos %f %f" % [x, y])
		#client.move_chunk_center(x, y)
		
func scale_to_world_space(x_vert, y_vert, z_vert):
	return Vector3(x_vert*GEOMETRY_SCALE-(bias_x), y_vert*GEOMETRY_HEIGHT, z_vert*-1*GEOMETRY_SCALE-(bias_z))

func scale_to_geo_space(x_vert, y_vert, z_vert):
	return Vector3(x_vert/GEOMETRY_SCALE+(bias_x), 0, (z_vert*-1)/GEOMETRY_SCALE+(bias_z))
