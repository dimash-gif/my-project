#version 130
in vec3 aPos;
in vec3 aNormal;
out vec3 vertexColor;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    vec3 lightPos = vec3(3.0, 2.0, 3.0);
    vec3 lightColor = vec3(1.0);
    vec3 norm = normalize(mat3(model) * aNormal);
    vec3 fragPos = vec3(model * vec4(aPos, 1.0));
    vec3 lightDir = normalize(lightPos - fragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vertexColor = diff * lightColor;
    gl_Position = projection * view * vec4(fragPos, 1.0);
}

