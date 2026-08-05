
# Goal: Given an image of a face, use MediaPipe face mesh to extract the 478 landmarks. Apply for all faces. Store results in a file.
import os
import numpy as np
import mediapipe as mp
import cv2
import pathlib as Path

num_undetected_faces = 0

# Initialize mediapipe face mesh model
def init_mp():
    mp_face_mesh = mp.solutions.face_mesh
    return mp_face_mesh.FaceMesh(
        static_image_mode=True,
        max_num_faces=1,
        refine_landmarks=True,
        min_detection_confidence=0.5
    )

# Store image using OpenCV, apply face mesh to image, obtain normalized coordinates (array), save the coordinates to a numpy array
def extract_landmarks(img_path, face_mesh):
    image = cv2.imread(str(img_path))
    if image is None:
        print(f"Unable to read image at {img_path}")
        return None

    results = face_mesh.process(cv2.cvtColor(image, cv2.COLOR_BGR2RGB))

    if not results.multi_face_landmarks:
        num_undetected_faces += 1
        print(f"Face detection failed for image at {img_path}!")

    # iterate over all face landmarks (sanity check)
    print("All faces detected: ")
    for face_landmarks in results.multi_face_landmarks:
        print('face_landmarks:', face_landmarks)

    face_landmarks = results.multi_face_landmarks[0]
    face_landmark_coords = [[lm.x, lm.y, lm.z] for lm in face_landmarks.landmark] # this list comprehension iterates through all 478 landmarks and saves them to a list

    return np.array(face_landmark_coords, dtype=np.float32) # a numpy array of shape (478, 3)

def generate_dataset():
    pass

# inside a try catch block (so I can see if unfortunatly any of the low res faces weren't detected)
# loop over all files in a folder:
    # call extract_landmarks()
    # send the data to some data structure -> .npz (numpy.savez_compressed -> )

# save the data structure to a file: 
# .pt if you dont care about caching the coordinate data, and just want to save it as a graph.
# .npz to keep the raw coordinate outputs as a seperate file, just make sure to convert to a .pt still when you run the model training script.

### This is the structure of what im doing in summary:
# raw images  -> extract_landmarks -> send to .npz file -> PyG InMemoryDataset creation -> final graph stored as .pt

# you should keep all of this logic in this file, and only load the .pt in the gcn.py file. no point in creating a seperate file for it.
# also note: you should use mediapipe's provided face mesh tesselation (edges/connections between the landmarks) or wireframe.
# this is because using k-nearest neighbours will build the vertex connections based on pure physical proximity, wheras vertex connections are based on anatomy
# if mediapipe's tesselation is used:
# tesselation = mp.solutions.face_mesh.FACEMESH_TESSELATION

