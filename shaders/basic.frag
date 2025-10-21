#version 130

in vec3 vNormal;
out vec4 FragColor;

void main() {
    // color = absolute value of normal to visualize direction as color
    vec3 n = normalize(vNormal);
    vec3 col = abs(n);        // values in [0,1]
    // optional small ambient base so darkest parts aren't pure black
    col = col * 0.9 + vec3(0.05);
    FragColor = vec4(col, 1.0);
}

