// CRT fragment shader (qsb source; compiled to crt.frag.qsb with the qsb tool:
//   qsb --qt6 -o crt.frag.qsb crt.frag
// Curvature: the classic "off = abs(uv.yx) / factors" barrel distortion, squared
// so the curve is strongest at the corners. Outside the curved mask there is no
// phosphor, so we paint the board colour. Chroma split grows toward the curved
// edges, scanlines run in screen space (one dark device line every 3), and a
// vignette darkens the corners).

#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D source;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
} ubuf;

vec2 curve(vec2 uv) {
    uv = uv * 2.0 - 1.0;
    vec2 off = abs(uv.yx) / vec2(4.5, 4.0);
    uv = uv + uv * off * off;
    return uv * 0.5 + 0.5;
}

void main() {
    vec2 uv = curve(qt_TexCoord0);
    vec2 v = qt_TexCoord0 - 0.5;
    // Distance to the curved mask edge, in curved-texture space: the rim and
    // the mask edge itself both key off it, so they follow the curvature.
    float d = min(min(uv.x, 1.0 - uv.x), min(uv.y, 1.0 - uv.y));
    // Outside the curved mask there is no phosphor; the boundary is blended
    // into the board colour over ~3 device px so it reads as glass, not a
    // sawtooth.
    float edge = smoothstep(0.0, 0.006, d);
    if (edge <= 0.0) {
        fragColor = vec4(0.020, 0.020, 0.028, 1.0) * ubuf.qt_Opacity;
        return;
    }
    vec4 c = texture(source, uv);
    // Chroma split grows toward the curved edges.
    vec2 shift = v * 0.004 * dot(v, v) * 4.0;
    c.r = texture(source, uv + shift).r;
    c.b = texture(source, uv - shift).b;
    // Scanlines in screen space: one dark row of 1px device line every 3.
    float scan = mod(gl_FragCoord.y, 3.0) < 1.0 ? 0.18 : 0.0;
    // Vignette: clear in the middle, dark corners.
    float vig = 1.0 - 0.5 * smoothstep(0.30, 0.85, length(v));
    vec3 col = c.rgb * (1.0 - scan) * vig;
    // The tube's own dark rim, ~9 device px, following the curve exactly:
    // this replaces the straight glass frame of the old layout.
    float rim = 1.0 - smoothstep(0.0, 0.020, d);
    col *= 1.0 - 0.5 * rim;
    col = mix(vec3(0.020, 0.020, 0.028), col, edge);
    fragColor = vec4(col, 1.0) * ubuf.qt_Opacity;
}
