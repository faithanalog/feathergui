TXT(
#version 110\n
varying vec2 uv;\n
varying vec4 color;\n

uniform sampler2D texture;\n

void main()\n
{\n
  gl_FragColor = texture2D(texture, uv).rgba * color;\n
  //gl_FragColor = vec4(uv.x*10.0, uv.y*10.0, 1, 1);\n
}\n
)