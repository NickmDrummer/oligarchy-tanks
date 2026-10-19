// CRT vertex shader (qsb source; compiled to crt.vert.qsb with the qsb tool:
//   qsb --qt6 -o crt.vert.qsb crt.vert
// qsb and the generated .qsb files are required because Qt 6 no longer accepts
// inline GLSL strings in ShaderEffect).

#version 440

layout(location = 0) in vec4 qt_Vertex;
layout(location = 1) in vec2 qt_MultiTexCoord0;
layout(location = 0) out vec2 qt_TexCoord0;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
} ubuf;

void main() {
    qt_TexCoord0 = qt_MultiTexCoord0;
    gl_Position = ubuf.qt_Matrix * qt_Vertex;
}
