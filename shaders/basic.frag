#version 130

in vec3 vNormal;
out vec4 FragColor;
uniform int uColorByNormal;

void main() {
    vec3 n = normalize(vNormal);
    vec3 col;
    if (uColorByNormal == 1) {
        // Rainbow from normals
        col = abs(n) * 0.9 + 0.1;
    } else {
        // Flat white
        col = vec3(1.0);
    }
    FragColor = vec4(col, 1.0);
}

