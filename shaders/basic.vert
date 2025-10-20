#version 130

in vec3 aPos;
in vec3 aNormal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3 vNormal;

void main() {
    // transform position
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    // compute normal matrix simply using transpose(model) for safety on GLSL 1.30
    mat3 normalMatrix = transpose(mat3(model));
    vNormal = normalize(normalMatrix * aNormal);
}

