"""Build the original first-person art as UE static meshes. Run in the Unreal editor."""
import math
import unreal as ue

ROOT = "/Game/ClassicFPS"
assets = ue.EditorAssetLibrary
tools = ue.AssetToolsHelpers.get_asset_tools()
V = ue.Vector

# Linear RGB plus a metallic mask. Parts share a single vertex-colour material.
steel = (.085, .104, .117, .72)
edge = (.22, .26, .28, .82)
dark = (.033, .040, .045, .38)
black = (.025, .031, .035, .03)
rubber = (.056, .062, .061, .02)
wood = (.30, .109, .042, .04)
grain = (.175, .055, .023, .04)
olive = (.103, .12, .068, .02)
cloth = (.091, .115, .106, .01)
leather = (.056, .049, .040, .04)
stitch = (.29, .22, .13, .04)
silver = (.45, .52, .54, .84)

def surface():
    name = "M_FieldFinish"
    if assets.does_asset_exist(ROOT + "/" + name):
        mat = assets.load_asset(ROOT + "/" + name)
        ed = ue.MaterialEditingLibrary
        color = ed.get_material_property_input_node(mat, ue.MaterialProperty.MP_METALLIC)
        assert color and ed.connect_material_property(color, "", ue.MaterialProperty.MP_BASE_COLOR)
        ed.recompile_material(mat)
        assets.save_loaded_asset(mat)
        return mat
    else:
        mat = tools.create_asset(name, ROOT, ue.Material, ue.MaterialFactoryNew())
    ed = ue.MaterialEditingLibrary
    color = ed.create_material_expression(mat, ue.MaterialExpressionVertexColor, -330, 0)
    assert ed.connect_material_property(color, "", ue.MaterialProperty.MP_BASE_COLOR)
    assert ed.connect_material_property(color, "A", ue.MaterialProperty.MP_METALLIC)
    rough = ed.create_material_expression(mat, ue.MaterialExpressionConstant, -320, 220)
    rough.set_editor_property("r", .48)
    assert ed.connect_material_property(rough, "", ue.MaterialProperty.MP_ROUGHNESS)
    ed.recompile_material(mat)
    assets.save_loaded_asset(mat)
    return mat

def mean(points):
    return tuple(sum(p[i] for p in points) / len(points) for i in range(3))

class Sculpt:
    def __init__(self):
        self.points, self.normals, self.colors, self.tris = [], [], [], []

    def face(self, pts, center, tint):
        a, b, c = [V(*p) for p in pts[:3]]
        normal = (b-a).cross(c-a).normal()
        if normal.dot(V(*mean(pts))-V(*center)) < 0:
            pts = list(reversed(pts))
            normal = normal * -1
        index = len(self.points)
        self.points.extend(V(*p) for p in pts)
        self.normals.extend([normal] * len(pts))
        self.colors.extend([ue.LinearColor(*tint)] * len(pts))
        self.tris.extend(ue.IntVector(index, index+i+1, index+i) for i in range(1,len(pts)-1))

    def loft(self, rings, tint):
        for a,b in zip(rings,rings[1:]):
            center = mean(a+b)
            for i in range(len(a)):
                j = (i+1) % len(a)
                self.face([a[i], a[j], b[j], b[i]], center, tint)
        self.face(rings[0], mean(rings[0]+rings[1]), tint)
        self.face(rings[-1], mean(rings[-2]+rings[-1]), tint)

    def tube(self, sections, tint, sides=12, phase=0):
        # Cross-sections follow the visual silhouette along the camera's X axis.
        self.loft([[(x,y+w*math.cos(phase+i*math.tau/sides),z+h*math.sin(phase+i*math.tau/sides))
                     for i in range(sides)] for x,y,z,w,h in sections], tint)

    def block(self, x1,x2,y1,y2,z1,z2,tint,bevel=.16):
        b=min(bevel,(y2-y1)*.31,(z2-z1)*.31)
        yz=[(y1+b,z1),(y2-b,z1),(y2,z1+b),(y2,z2-b),(y2-b,z2),(y1+b,z2),(y1,z2-b),(y1,z1+b)]
        self.loft([[(x,y,z) for y,z in yz] for x in (x1,x2)],tint)

    def profile(self, sections, tint):
        # Extruded strips with a chamfer along the two visible side panels.
        rings=[]
        for x,lo,hi,w in sections:
            b=min(.32,(hi-lo)*.24,w*.25)
            rings.append([(x,y,z) for y,z in [(-w+b,lo),(w-b,lo),(w,lo+b),(w,hi-b),(w-b,hi),(-w+b,hi),(-w,hi-b),(-w,lo+b)]])
        self.loft(rings,tint)

    def save(self,name):
        path=ROOT+"/Meshes/SM_"+name
        buf=ue.GeometryScriptSimpleMeshBuffers()
        buf.vertices,buf.normals,buf.vertex_colors,buf.triangles=self.points,self.normals,self.colors,self.tris
        buf.uv0=[ue.Vector2D(p.y if abs(n.x) > .7 else p.x, p.y if abs(n.z) > .7 else p.z) * .05
                 for p,n in zip(self.points,self.normals)]
        mesh=ue.DynamicMesh()
        ue.GeometryScript_MeshEdits.append_buffers_to_mesh(mesh,buf)
        options=ue.GeometryScriptCreateNewStaticMeshAssetOptions()
        options.enable_collision=False
        if assets.does_asset_exist(path):
            static=assets.load_asset(path)
            _,result=ue.GeometryScript_AssetUtils.copy_mesh_to_static_mesh(
                mesh,static,ue.GeometryScriptCopyMeshToAssetOptions(),ue.GeometryScriptMeshWriteLOD())
            assert result==ue.GeometryScriptOutcomePins.SUCCESS,path
        else:
            static,_=ue.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(mesh,path,options)
        assert static, path
        static.set_material(0,finish)
        assert assets.save_loaded_asset(static),path

def ak():
    s=Sculpt()
    s.profile([(-23,-6,1,2.2),(-21,-6.4,1.3,2.4),(-12,-4,1.5,1.6),(-7,-2.8,2.4,1.45),(-2,-2,2.6,1.6)],wood)
    s.profile([(-23.7,-6.1,1.1,2.25),(-23,-6.2,1.1,2.3)],dark)
    s.block(-3,19,-1.8,1.8,-2,2.9,steel)
    s.tube([(-3,0,2.5,1.7,1.25),(1,0,2.6,1.75,1.55),(16,0,2.6,1.6,1.35),(19,0,2.5,1.4,1.2)],dark)
    for x in (1,5,9):
        s.block(x,x+.26,-1.75,1.75,2.2,3.68,steel)
    s.profile([(-4,-9,-7.9,1.25),(-2.1,-9.3,-1.7,1.5),(1.8,-7.9,-1.4,1.5),(3.2,-2.5,-1.6,1.35)],grain)
    s.block(2,7,-.45,.45,-5.4,-4.8,dark)
    s.block(6.6,7.3,-.45,.45,-5,-1.8,dark)
    s.profile([(18,-2,2.6,1.45),(20,-2.4,2.9,2.08),(30,-2.1,2.85,2),(33,-1.5,2.25,1.55)],wood)
    s.tube([(18,0,3.1,1.2,1),(30,0,3.1,1.25,1),(32,0,2.7,1,.9)],grain)
    for x in (20,23,26,29):
        for sign in (-1,1):
            s.block(x,x+1.15,sign*1.89-.12,sign*1.89+.12,1.8,2.45,dark)
    s.tube([(32,0,.6,.94,.94),(48,0,.6,.80,.80),(51,0,.6,.95,.95)],steel)
    s.profile([(33,1.0,3.4,.8),(35,1.0,3.55,.8),(39,.95,1.6,.65)],dark)
    s.profile([(45,1.1,2.3,.58),(46.8,1.1,5.5,.62),(48.1,1.1,5.7,.6)],dark)
    for side in (-1,1):
        s.block(46.9,48.4,side*.85-.14,side*.85+.14,5,6.1,steel)
    s.block(13,17,-.68,.68,3.7,4.55,edge)
    for x in (0,3,12.5,16.5):
        for side in (-1,1):
            s.block(x,x+.46,side*1.85-.1,side*1.85+.1,-.5,.08,edge)
    s.save("AK")
    m=Sculpt()
    def mag_slice(x,z,length,width):
        return [(x+a,y,z) for a,y in [(-length+.5,-width),(length-.5,-width),(length,-width+.4),(length,width-.4),(length-.5,width),(-length+.5,width),(-length,width-.4),(-length,-width+.4)]]
    path=[(11.8,-1.5),(11.9,-5),(12.6,-8.4),(14.1,-12),(16.1,-15.2)]
    m.loft([mag_slice(x,z,3.35,1.48) for x,z in path],steel)
    for side in (-1,1):
        for off in (-1.7,0,1.7):
            m.loft([[(x+off+a,side*1.52+b,z) for a,b in [(-.2,-.12),(.2,-.12),(.2,.12),(-.2,.12)]] for x,z in path],edge)
    m.save("AK_Mag")

def usp():
    s=Sculpt()
    slide=Sculpt()
    slide.profile([(-4,-1.05,2.55,1.37),(-3.5,-1.3,2.92,1.54),(11,-1.24,2.93,1.48),(12,-1,2.65,1.36)],steel)
    s.profile([(-4,-2,-.85,1.32),(-3,-2.4,-.8,1.4),(10.4,-2.6,-.8,1.29)],black)
    s.profile([(-5.3,-10,-8.9,1.14),(-4,-10.1,-1.4,1.42),(-1,-9.8,-1.6,1.38),(1.1,-2.9,-1.5,1.23)],rubber)
    s.block(0,5,-.6,.6,-5.2,-4.55,black)
    s.block(4.6,5.25,-.6,.6,-4.8,-2,black)
    for x in (-2.9,-2.35,-1.8,-1.25,-.7):
        for side in (-1,1):
            slide.block(x,x+.21,side*1.51-.075,side*1.51+.075,.3,2.22,dark)
    for x in (-2.5,10.1):
        slide.block(x,x+.8,-.42,.42,2.82,3.47,dark)
    slide.save("USP_Slide")
    s.tube([(8,0,1.55,.81,.81),(13.4,0,1.55,.85,.85)],steel,16)
    can=Sculpt()
    can.tube([(13.15,0,1.55,.86,.86),(14,0,1.55,1.32,1.32),(28,0,1.55,1.32,1.32),(28.4,0,1.55,1.16,1.16)],dark,20)
    for x in (14.6,15.1,25.4,26,26.6):
        can.tube([(x,0,1.55,1.35,1.35),(x+.14,0,1.55,1.35,1.35)],steel,20)
    can.save("USP_Suppressor")
    s.save("USP")
    m=Sculpt()
    m.profile([(-5.2,-10.6,-9.8,1.31),(-4.7,-10.8,-3.8,1.3),(-1.7,-10.6,-3.5,1.26),(-1,-10.4,-9.8,1.21)],dark)
    m.save("USP_Mag")

def knife():
    s=Sculpt()
    s.tube([(-8,0,0,1.1,1.45),(-7,0,0,1.45,1.7),(1,0,0,1.43,1.65),(3,0,0,1.25,1.53)],olive)
    for x in (-7,-5.5,-4,-2.5,-1,.5,2):
        s.tube([(x,0,0,1.52,1.73),(x+.42,0,0,1.52,1.73)],rubber)
    s.block(2.6,3.45,-1.15,1.15,-2.65,2.85,steel)
    # Guard ring and a broad clip-point silhouette with a separate polished bevel.
    for i in range(14):
        a,b=i*math.tau/14,(i+1)*math.tau/14
        s.loft([[(x,r*math.cos(t),3.55+r*math.sin(t)) for x,r in [(2.62,.78),(3.43,.78),(3.43,1.18),(2.62,1.18)]] for t in (a,b)],steel)
    blade=[(3.4,-1.7,1.25),(5,-2.0,2.15),(9,-2.07,2.2),(16,-1.94,2.2),(19,-1.55,2.15),(22,-.75,1.42),(25.1,.45,.53)]
    for (x,lo,top),(xx,ll,tt) in zip(blade,blade[1:]):
        def ring(x,l,h):
            w=min(.43,(h-l)*.32)
            return [(x,0,l),(x,w,l+(h-l)*.39),(x,w*.54,h),(x,-w*.54,h),(x,-w,l+(h-l)*.39)]
        a,b=ring(x,lo,top),ring(xx,ll,tt)
        center=mean(a+b)
        for i,col in enumerate([silver,steel,edge,steel,silver]):
            j=(i+1)%5
            s.face([a[i],a[j],b[j],b[i]],center,col)
    for x in (6,7.4,8.8,10.2,11.6,13):
        s.profile([(x,2.02,2.29,.19),(x+.33,2.06,2.48,.20),(x+.72,2.06,2.20,.19)],dark)
    s.save("M9")

def glove(name,x,y,z,left=False):
    s=Sculpt()
    side=-1 if left else 1
    elbow=(-25,side*16,-15)
    wrist=(x-4,y+side*1.4,z-1.9)
    s.tube([(*elbow,4.9,5.0),(x-11,y+side*4,z-4.8,4.15,3.95),(*wrist,2.85,2.5)],cloth,12)
    s.tube([(wrist[0]-2.3,wrist[1]+side*.6,wrist[2]-.7,3.23,2.85),(*wrist,3.13,2.72)],black)
    s.tube([(*wrist,2.85,2.45),(x,y,z,2.28,2.8),(x+2.2,y-side*.4,z+.3,1.65,2.28)],leather,12)
    for i in range(4):
        fx=x+.65-i*.37
        fz=z+1.65-i*1.12
        s.tube([(fx,y,fz,.78,.53),(fx+2.05,y-side*.42,fz,.85,.59),(fx+2.75,y-side*1.53,fz+.02,.70,.51),(fx+2.93,y-side*2.55,fz+.24,.43,.45)],rubber,8)
        s.block(fx+.35,fx+1.4,y+side*1.60-.13,y+side*1.60+.13,fz-.23,fz+.2,black)
    s.tube([(x-2.0,y-side*.6,z+.45,.92,1.08),(x+.2,y-side*1.55,z+2.30,.87,.76),(x+2.7,y-side*1.90,z+2.38,.62,.59)],rubber,10)
    for n in range(3):
        s.block(x-4.3+n*.62,x-4.04+n*.62,y+side*2.7-.12,y+side*2.7+.12,z-.4,z+.16,stitch)
    s.save(name)

assets.make_directory(ROOT+"/Meshes")
finish=surface()
ak()
usp()
knife()
glove("RightGlove",-2,2.5,-5.1)
glove("RifleSupport",24,-2.7,-3.1,True)
glove("PistolSupport",-2,-2.2,-5.8,True)
glove("KnifeSupport",6,-19,-4.4,True)
ue.log("CLASSIC_MODELS_READY")
