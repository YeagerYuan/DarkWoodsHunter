"""Create the walkable single-player practice level in a fresh editor process."""
import unreal as ue

PATH="/Game/ClassicFPS/Lvl_ClassicRange"
assets=ue.EditorAssetLibrary
levels=ue.get_editor_subsystem(ue.LevelEditorSubsystem)
actors=ue.get_editor_subsystem(ue.EditorActorSubsystem)
if assets.does_asset_exist(PATH):
    raise RuntimeError("Level already exists; refusing to replace an edited range.")
assert levels.new_level(PATH)
cube=assets.load_asset("/Engine/BasicShapes/Cube")
base=assets.load_asset("/Engine/BasicShapes/BasicShapeMaterial")
tools=ue.AssetToolsHelpers.get_asset_tools()
palette={}
for name,rgb in {"Concrete":(.26,.29,.26),"Pine":(.063,.119,.106),"Timber":(.285,.17,.095),
                 "Graphite":(.05,.065,.064),"Amber":(.91,.40,.087),"Paint":(.70,.68,.52)}.items():
    folder="/Game/ClassicFPS/RangeMaterials"
    mat=assets.load_asset(folder+"/MI_"+name) if assets.does_asset_exist(folder+"/MI_"+name) else None
    if not mat:
        mat=tools.create_asset("MI_"+name,folder,ue.MaterialInstanceConstant,ue.MaterialInstanceConstantFactoryNew())
        ue.MaterialEditingLibrary.set_material_instance_parent(mat,base)
        ue.MaterialEditingLibrary.set_material_instance_vector_parameter_value(mat,"Color",ue.LinearColor(*rgb))
        assets.save_loaded_asset(mat)
    palette[name]=mat

def spawn(cls,loc,rot=None):
    a=actors.spawn_actor_from_class(cls,ue.Vector(*loc),rot or ue.Rotator())
    assert a,cls
    return a

def box(label,loc,size,mat):
    a=spawn(ue.StaticMeshActor,loc)
    comp=a.static_mesh_component
    comp.set_static_mesh(cube)
    comp.set_material(0,palette[mat])
    a.set_actor_scale3d(ue.Vector(*(s/100 for s in size)))
    a.set_actor_label(label)
    return a

def stencil(label,loc,size):
    a=spawn(ue.TextRenderActor,loc,ue.Rotator(yaw=180))
    c=a.get_component_by_class(ue.TextRenderComponent)
    c.set_text(label)
    c.set_world_size(size)
    c.set_horizontal_alignment(ue.HorizTextAligment.EHTA_CENTER)
    c.set_text_render_color(ue.Color(237,221,178,255))
    a.set_actor_label(label)

world=ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
world.get_world_settings().set_editor_property("default_game_mode",ue.load_class(None,"/Script/ClassicLoadout.ClassicRangeMode"))
spawn(ue.PlayerStart,(0,0,101))
box("Apron",(650,0,-18),(2600,1700,36),"Concrete")
box("Backstop",(1910,0,245),(48,1700,490),"Pine")
box("Entrance wall",(-625,0,180),(40,1700,360),"Pine")
for side in (-1,1):
    box("Side wall",(650,side*843,135),(2600,32,270),"Pine")
    box("Wall cap",(650,side*843,274),(2600,46,9),"Timber")
    for x in (-510,60,640,1220,1790):
        box("Pier",(x,side*818,140),(26,35,280),"Graphite")
for y in range(-800,801,80):
    box("Timber baffle",(1880,y,167),(11,69,326),"Timber")
box("Header rail",(1870,0,332),(18,1700,8),"Amber")
stencil("D A R K   W O O D S",(1878,0,445),52)
stencil("P R A C T I C E    R A N G E",(1878,0,382),23)
for y in (-480,-160,160,480):
    box("Lane stripe",(685,y,.35),(1960,3,.65),"Paint")
for x in (230,680,1130,1580):
    for y in (-640,-320,0,320,640):
        box("Lane hash",(x,y,.65),(3,29,.9),"Amber")
target=ue.load_class(None,"/Script/ClassicLoadout.ClassicRangeTarget")
for i,(x,y,z) in enumerate([(360,-360,142),(875,0,163),(1320,360,178),(1650,-550,205),(1680,600,120)]):
    box("Stand",(x+9,y,z*.48),(12,10,z*.96),"Graphite")
    box("Foot",(x+8,y,5),(66,67,10),"Timber")
    t=spawn(target,(x,y,z))
    t.set_actor_label("Plate %02d"%(i+1))
    stencil("%02d"%(i+1),(x-2,y,63),18)
sun=spawn(ue.DirectionalLight,(0,0,750),ue.Rotator(pitch=-48,yaw=26))
sun.light_component.set_editor_property("intensity",4.5)
sun.light_component.set_editor_property("atmosphere_sun_light",True)
spawn(ue.SkyAtmosphere,(0,0,0))
sky=spawn(ue.SkyLight,(0,0,350))
sky.light_component.set_editor_property("real_time_capture",True)
sky.light_component.set_editor_property("intensity",1.15)
# Keep the hands and ironwork legible in the shade as the player turns around.
fill=spawn(ue.DirectionalLight,(0,0,600),ue.Rotator(pitch=-32,yaw=196))
fill.light_component.set_editor_property("intensity",1.35)
fill.light_component.set_editor_property("cast_shadows",False)
fill.light_component.set_editor_property("atmosphere_sun_light",False)
assert levels.save_current_level()
ue.log("CLASSIC_RANGE_READY")
