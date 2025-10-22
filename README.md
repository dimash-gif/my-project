# OpenGL Interactive Graphics Projects

## Overview
This repository contains two major OpenGL programs demonstrating interactive 3D computer graphics concepts. We wrote several programs, which loads a triangle mesh model from an smf file, and displays 3D figures in a window.

## How to build and run the code
Firstly, download all files from repository, and unzip the file.

To build 
```bash 
make
```

To run the code
```bash
./smf_viewer models/bound-lo-sphere.smf //for the part 1
./shading_demo models/bound-lo-sphere.smf //for the part 2
```
# Controls

## Camera Controls
| Key / Action | Description |
|---------------|-------------|
| **A / D** | Rotate camera around the object (orbit left/right) |
| **W / S** | Zoom in / out |
| **Q / E** | Raise / lower camera height |
| **P** | Toggle between Perspective and Orthographic projection |

## Light Controls
| Key | Action |
|------|--------|
| **← / →** | Rotate world light around the model (adjust light angle) |
| **I / K** | Move world light closer / farther (change light radius) |
| **U / O** | Raise / lower world light height |
| **ESC** | Exit program |

## Light Controls
| Key | Action |
|------|--------|
| **G** | Toggle between Gouraud and Phong shading |
| **1** | Apply Red Plastic material (bright specular highlight) |
| **2** | Apply Emerald material (green reflective look) |
| **3** | Apply Cyan Rubber material (soft matte finish) |

## General
| Key | Action |
|------|--------|
| **ESC** | Exit the program |


