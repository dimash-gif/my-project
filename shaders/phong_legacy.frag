#version 130
in vec3 FragPos;
in vec3 Normal;
out vec4 FragColor;

void main() {
    vec3 norm = normalize(Normal);
    vec3 lightPos = vec3(3.0, 2.0, 3.0);
    vec3 viewPos = vec3(0.0, 0.0, 5.0);
    vec3 lightColor = vec3(1.0);

    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);

    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);

    vec3 color = (0.1 + diff + spec) * lightColor;
    FragColor = vec4(color, 1.0);
}

