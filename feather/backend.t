local C = terralib.includecstring [[
  #include <string.h>
]]

local B = {}
local F = require 'feather.shared'
local Flags = require 'feather.flags'
local Enum = require 'feather.enum'
local OS = require 'feather.os'
local Alloc = require 'std.alloc'
local Msg = require 'feather.message'
local Log = require 'feather.log'
local C = require 'feather.libc'
local tunpack = table.unpack or unpack

B.Feature = Flags{
  "TEXT_ANTIALIAS", 
  "TEXT_SUBPIXEL",
  "TEXT_BLUR",
  "TEXT_ALPHA",
  "SHAPE_BLUR",
  "SHAPE_ALPHA",
  "RECT_CORNERS",
  "RECT_BORDER",
  "CIRCLE_INNER",
  "CIRCLE_BORDER",
  "ARC_INNER",
  "ARC_BORDER",
  "TRIANGLE_CORNERS",
  "TRIANGLE_BORDER",
  "LINES_ALPHA",
  "CURVE_STROKE", -- If both stroke and fill are false, it doesn't support curves at all
  "CURVE_FILL",
  "LAYER_TRANSFORM",
  "LAYER_OPACITY",
  "GAMMA",
  "BATCHING", -- If true, the backend is capable of batching certain command lists together. 
  "SHADER_GLSL_ES2", -- Shader type determines what CreateShader will accept
  "SHADER_GLSL4",
  "SHADER_HLSL2",
  "BACKGROUND_OPACITY",
  "IMMEDIATE_MODE"}

B.Format = Enum{
  "GRADIENT",
  "BUFFER",
  "LAYER",
  "WEAK_LAYER",
  "ATLAS",
  "BMP",
  "JPG",
  "PNG",
  "ICO",
  "GIF",
  "TIFF",
  "TGA",
  "WEBP",
  "DDS",
  "WIC",
  "SVG",
  "AVI",
  "MP4",
  "MKV",
  "WEBM"}

B.Format.methods["UNKNOWN"] = constant(`0xff)
B.Format.enum_values["UNKNOWN"] = 0xff

B.AntiAliasing = Flags{
  "AA",
  "LCD",
  "LCD_V",
  "SDF",
}

B.BreakStyle = Enum{
  "NONE",
  "WORD",
  "CHARACTER",
}

B.Clipboard = Enum{
  "NONE",
  "TEXT",
  "WAVE",
  "BITMAP",
  "FILE",
  "ELEMENT",
  "CUSTOM",
  "ALL",
}

B.Cursor = Enum{
  "NONE",
  "ARROW",
  "IBEAM",
  "CROSS",
  "WAIT",
  "HAND",
  "RESIZENS",
  "RESIZEWE",
  "RESIZENWSE",
  "RESIZENESW",
  "RESIZEALL",
  "NO",
  "HELP",
  "DRAG",
  "CUSTOM",
}


B.DrawFlags = Flags({
  "CCW_FRONT_FACE",
  "CULL_FACE",
  "WIREFRAME",
  "POINTMODE",
  "INSTANCED",
  "LINEAR",
}, uint8)

struct B.Data {
  union {
    data : &opaque
    index : uint
  }
}

struct B.Font {
  data : B.Data
  dpi : F.Vec
  baseline : float
  lineheight : float
  pt : uint
  aa : B.AntiAliasing
}

B.Primitive = Enum({
  "POINT",
  "LINE",
  "TRIANGLE",
  "LINE_STRIP",
  "TRIANGLE_STRIP",
  "LINE_ADJACENCY",
  "TRIANGLE_ADJACENCY",
  "LINE_STRIP_ADJACENCY",
  "TRIANGLE_STRIP_ADJACENCY",
  "INDEX_BYTE",
  "INDEX_SHORT",
  "INDEX_INT",
}, uint8)

B.ShaderType = Enum{
  "HALF",
  "FLOAT",
  "DOUBLE",
  "INT",
  "UINT",
  "COLOR32",
  "TEXTURE",
  "TEXCUBE",
}

struct B.ShaderParameter {
  type : B.ShaderType
  length : uint -- for arrays
  multi : uint -- for matrices, or to indicate TEX1D, TEX2D, TEX3D
  name : F.conststring
}

B.AssetFlags = Flags({
  "LINEAR",
  "NO_MIPMAP",
  "CACHE_LAYER",
  "CUBE_MAP",
})

struct B.Asset {
  data : B.Data
  format : B.Format
  flags : B.AssetFlags
  union {
    texture : struct {
      size : F.Veci
      dpi : F.Vec
    }
    resource : struct {
      count : uint
      stride : uint16
      primitive : uint8
      parameters : &B.ShaderParameter
      n_parameters : uint
    }
  }
}
B.Asset.c_export = [[FG_Data data;
  FG_Format format;
  int flags;
  union {
    struct {
      FG_Veci size;
      FG_Vec dpi;
    };
    struct {
      unsigned int count;
      unsigned short stride;
      unsigned char primitive;
      FG_ShaderParameter * parameters;
      unsigned int n_parameters;
    };
  };]]

struct B.ShaderValue {
  union {
    f32 : float
    f64 : double
    i32 : int
    u32 : uint
    pf32 : &float
    pf64 : &double
    pi32 : &int
    pu32 : &uint
    asset : &B.Asset
  }
}

struct B.Shader {
  data : &opaque
  parameters : &B.ShaderParameter
  n_parameters : uint
}

struct B.Display {
  size : F.Veci
  offset : F.Veci
  dpi : F.Vec
  scale : float
  handle : &opaque
  primary : bool
}

B.BlendValue = Enum({
  "ZERO", 
  "ONE", 
  "SRC_COLOR", 
  "INV_SRC_COLOR",
  "DST_COLOR", 
  "INV_DST_COLOR", 
  "SRC_ALPHA",
  "INV_SRC_ALPHA",
  "DST_ALPHA", 
  "INV_DST_ALPHA",
  "CONSTANT_COLOR", 
  "INV_CONSTANT_COLOR",
  "CONSTANT_ALPHA", 
  "INV_CONSTANT_ALPHA",
  "SRC_ALPHA_SATURATE",
}, uint8)

B.BlendOp = Enum({
  "ADD",
  "SUBTRACT",
  "REV_SUBTRACT",
}, uint8)

struct B.BlendState {
  srcBlend : B.BlendValue
  destBlend : B.BlendValue
  colorBlend : B.BlendOp
  srcBlendAlpha : B.BlendValue
  destBlendAlpha : B.BlendValue
  alphaBlend : B.BlendOp
  mask : uint8 -- RGBA mask, R is bit 1, A is bit 4
  flags : B.DrawFlags
  constant : F.Color
}

B.Category = Enum({
  "TEXT",
  "ASSET",
  "RECT",
  "CIRCLE",
  "ARC",
  "TRIANGLE",
  "LINES",
  "CURVE",
  "LINES3D",
  "CURVE3D",
  "CUBE",
  "ICOSPHERE",
  "CYLINDER",
  "SHADER"
}, uint8)

struct B.Command {
  category : B.Category
  union {
    text : struct { 
      font : &B.Font, 
      layout : &opaque, 
      area : &F.Rect, 
      color : F.Color,
      blur : float, 
      rotate : float, 
      z : float 
    }
    asset : struct { 
      asset : &B.Asset, 
      area : &F.Rect, 
      source : &F.Rect, 
      color : F.Color, 
      time : float, 
      rotate : float, 
      z : float 
    }
    shape : struct {
      area : &F.Rect, 
      fillColor : F.Color
      border : float
      borderColor : F.Color
      blur : float, 
      asset : &B.Asset
      z : float
      union {
        rect : struct {
          corners : &F.Rect, 
          rotate : float, 
        }
        circle : struct {
          innerRadius : float
          innerBorder : float
        }
        arc : struct {
          angles : F.Vec
          innerRadius : float
        }
        triangle : struct {
          corners : &F.Rect
          rotate : float
        }
      }
    }
    lines : struct {
      union {
        points : &F.Vec
        points3D : &F.Vec3
      }
      count : uint
      color : F.Color
    }
    curve : struct {
      union {
        points : &F.Vec
        points3D : &F.Vec3
      }
      count : uint
      fillColor : F.Color
      stroke : float
      strokeColor : F.Color
    }
    shape3D : struct {
      shader : &B.Shader
      subdivision : int
      values : &B.ShaderValue
    }
    shader : struct {
      shader : &B.Shader
      vertices : &B.Asset
      indices : &B.Asset
      values : &B.ShaderValue
    }
  }
}

struct B.Backend {
  destroy : {&B.Backend} -> {}

  features : B.Feature
  formats : uint
  dpi : F.Vec
  scale : float
  cursorblink : uint64
  tooltipdelay : uint64
}

-- Define a dynamic backend object (a static backend would be a seperate type that also provides these functions).
terra B.Backend:Draw(window : &Msg.Window, commands : &B.Command, n_commands : uint, blendstate : &B.BlendState) : F.Err return 0 end
terra B.Backend:Clear(window : &Msg.Window, color : F.Color) : bool return false end -- Clears whatever is inside the current clipping rect
terra B.Backend:PushLayer(window : &Msg.Window, layer : &B.Asset, transform : &float, opacity : float, blendstate : &B.BlendState) : F.Err return 0 end
terra B.Backend:PopLayer(window : &Msg.Window) : F.Err return 0 end
terra B.Backend:SetRenderTarget(window : &Msg.Window, target : &B.Asset) : F.Err return 0 end
terra B.Backend:PushClip(window : &Msg.Window, area : &F.Rect) : F.Err return 0 end
terra B.Backend:PopClip(window : &Msg.Window) : F.Err return 0 end
terra B.Backend:DirtyRect(window : &Msg.Window, area : &F.Rect) : F.Err return 0 end
terra B.Backend:BeginDraw(window : &Msg.Window, area : &F.Rect) : F.Err return 0 end
terra B.Backend:EndDraw(window : &Msg.Window) : F.Err return 0 end

terra B.Backend:CreateShader(ps : F.conststring, vs : F.conststring, gs : F.conststring, cs : F.conststring, ds : F.conststring, hs : F.conststring, parameters : &B.ShaderParameter, n_parameters : uint) : &B.Shader return nil end
terra B.Backend:DestroyShader(shader : &B.Shader) : F.Err return 0 end

terra B.Backend:CreateFont(family : F.conststring, weight : uint16, italic : bool, pt : uint32, dpi : F.Vec, aa : B.AntiAliasing) : &B.Font return nil end
terra B.Backend:DestroyFont(font : &B.Font) : F.Err return 0 end
terra B.Backend:FontLayout(font : &B.Font, text : F.conststring, area : &F.Rect, lineHeight : float, letterSpacing : float, breakStyle : B.BreakStyle, previous : &opaque) : &opaque return nil end
terra B.Backend:DestroyLayout(layout : &opaque) : F.Err return 0 end
terra B.Backend:FontIndex(font : &B.Font, layout : &opaque, area : &F.Rect, pos : F.Vec, cursor : &F.Vec) : uint return 0 end
terra B.Backend:FontPos(font : &B.Font, layout : &opaque, area :&F.Rect, index : uint) : F.Vec return F.Vec{} end

terra B.Backend:CreateAsset(data : F.conststring, count : uint, format : B.Format, flags : int) : &B.Asset return nil end
terra B.Backend:CreateBuffer(data : &opaque, bytes : uint, primitive : B.Primitive, parameters : &B.ShaderParameter, n_parameters : uint) : &B.Asset return nil end
terra B.Backend:CreateLayer(window : &Msg.Window, size : &F.Vec, flags : int) : &B.Asset return nil end
--terra B.Backend:CreateAtlas(assets : &B.Asset, count : uint, flags : int) : &B.Asset return nil end
--terra B.Backend:AddAtlas(atlas : &B.Asset, assets : &B.Asset, count : uint, flags : int) : F.Err return 0 end
--terra B.Backend:RemoveAtlas(atlas : &B.Asset, assets : &B.Asset, count : uint, flags : int) : F.Err return 0 end
terra B.Backend:DestroyAsset(asset : &B.Asset) : F.Err return 0 end
terra B.Backend:GetProjection(window : &Msg.Window, layer : &B.Asset, proj4x4 : &float) : F.Err return 0 end

terra B.Backend:PutClipboard(window : &Msg.Window, kind : B.Clipboard, data : F.conststring, count : uint) : F.Err return 0 end
terra B.Backend:GetClipboard(window : &Msg.Window, kind : B.Clipboard, target : &opaque, count : uint) : uint return 0 end
terra B.Backend:CheckClipboard(window : &Msg.Window, kind : B.Clipboard) : bool return false end
terra B.Backend:ClearClipboard(window : &Msg.Window, kind : B.Clipboard) : F.Err return 0 end

terra B.Backend:CreateSystemControl(window : &Msg.Window, id : F.conststring, area : &F.Rect, ...) : &opaque return nil end
terra B.Backend:SetSystemControl(window : &Msg.Window, control : &opaque, area : &F.Rect, ...) : F.Err return 0 end
terra B.Backend:DestroySystemControl(window : &Msg.Window, control : &opaque) : F.Err return 0 end

terra B.Backend:GetSyncObject() : &opaque return nil end
terra B.Backend:ProcessMessages() : F.Err return 0 end
terra B.Backend:SetCursor(window : &Msg.Window, cursor : B.Cursor) : F.Err return 0 end
terra B.Backend:GetDisplayIndex(index : uint, out : &B.Display) : F.Err return 0 end
terra B.Backend:GetDisplay(handle : &opaque, out : &B.Display) : F.Err return 0 end
terra B.Backend:GetDisplayWindow(window : &Msg.Window, out : &B.Display) : F.Err return 0 end
terra B.Backend:CreateRegion(element : &Msg.Receiver, window : Msg.Window, pos : F.Vec3, dim : F.Vec3) : &Msg.Window return nil end
terra B.Backend:CreateWindow(element : &Msg.Receiver, display : &opaque, pos : &F.Vec, dim : &F.Vec, caption : F.conststring, flags : uint64) : &Msg.Window return nil end
terra B.Backend:SetWindow(window : &Msg.Window, element : &Msg.Receiver, display : &opaque, pos : &F.Vec, dim : &F.Vec, caption : F.conststring, flags : uint64) : F.Err return 0 end
terra B.Backend:DestroyWindow(window : &Msg.Window) : F.Err return 0 end

-- Generate both the function pointer field and redefine the function to call the generated function pointer
do
  local map = {}
  for k, v in pairs(B.Backend.methods) do
    klow = string.lower(k:sub(1,1)) .. k:sub(2, -1)
    v.c_body = "return (*self->"..klow..")("

    for i, p in ipairs(v.definition.parameters) do
      if i > 1 then 
        v.c_body = v.c_body .. ", "
      end
      v.c_body = v.c_body .. p.name
    end
    
    v.c_body = v.c_body .. ");"

    v.type.parameters:map(symbol)
    B[k] = v
    B.Backend.entries:insert({field = klow, type = terralib.types.pointer(v:gettype())})
    
    map[klow] = v -- TODO: alphabetically sort map before putting in entries, or use a C struct sorting method on the resulting struct
  end

  for k, v in pairs(map) do
    local args = v.definition.parameters:map(function(x) return x.symbol end)
    if v.type.isvararg then
      v:resetdefinition(terra([args], ...): v.type.returntype return [args[1]].[k]([args]) end)
    else
      v:resetdefinition(terra([args]): v.type.returntype return [args[1]].[k]([args]) end)
    end
  end
end

terra B.Backend:free(library : &opaque) : {}
  self.destroy(self)
  if library ~= nil then 
    OS.FreeLibrary(library)
  end
end

B.Log = terralib.types.funcpointer({&opaque, Log.Level, F.conststring}, {}, true)
B.InitBackend = {&opaque, B.Log, Msg.Behavior} -> &B.Backend

terra LoadDynamicBackend(ui : &opaque, behavior : Msg.Behavior, log : B.Log, path : rawstring, name : rawstring) : {&B.Backend, &opaque}
  var l : &opaque = OS.LoadLibrary(path)

  if l == nil then
    C.printf("Error loading %s: %s\n", path, OS.LoadDLLError())
    return nil, nil
  end

  var str : rawstring = nil
  if name == nil then
    name = C.strrchr(path, ("\\")[0])
    if name == nil then
      name = C.strrchr(path, ("/")[0])
    end
    name = terralib.select(name == nil, path, name + 1)

    if C.strncmp(name, "lib", 3) == 0 then
      name = name + 3
    end

    str = Alloc.default_allocator:alloc(int8, C.strlen(name) + 1)
    C.strcpy(str, name)
    name = str

    var ext : rawstring = C.strrchr(str, (".")[0])
    if ext ~= nil then
      @ext = 0
    end

    -- strip _d from the name
    var sub : rawstring = C.strrchr(str, ("_")[0])
    if sub ~= nil and sub[1] ~= 0 and sub[1] == ("d")[0] then
      @sub = 0
    end
  end

  var f : B.InitBackend = [B.InitBackend](OS.LoadFunction(l, name))

  defer [terra (s : rawstring) : {} if s ~= nil then Alloc.free(s) end end](str)

  if f ~= nil then
    var b : &B.Backend = f(ui, log, behavior)
    if b ~= nil then
      return b, l
    end
    C.printf("Init function failed in %s", path)
  else
    C.printf("Can't find init function %s() in %s", name, path)
  end

  OS.FreeLibrary(l)
  return nil, nil
end

-- In cases where the backend is not known at compile time, we must load it via a shared library at runtime 
terra B.Backend.methods.new(ui : &opaque, behavior : Msg.Behavior, log : B.Log, path : rawstring, name : rawstring) : {&B.Backend, &opaque}
  return LoadDynamicBackend(ui, behavior, log, path, name)
end

return B