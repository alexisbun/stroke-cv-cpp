# Goal: Given an image of a face, use MediaPipe face mesh to extract the 478 landmarks. Apply for all faces. Store results in a file.

# This is the structure of what im doing in summary:
# raw images  -> extract_landmarks -> send to .npz file -> PyG InMemoryDataset creation -> final graph stored as .pt
import os
import re
import numpy as np
import mediapipe as mp
from mediapipe.tasks import python
from mediapipe.tasks.python import vision
import cv2
from pathlib import Path

# Initialize mediapipe face mesh model
def init_mp():
    base_options = python.BaseOptions(model_asset_path='/home/alexis/git/stroke-cv-cpp/flutter_ui/assets/face_landmarker.task')
    options = vision.FaceLandmarkerOptions(
        base_options=base_options,
        output_face_blendshapes=True,
        output_facial_transformation_matrixes=True,
        num_faces=1)
    detector = vision.FaceLandmarker.create_from_options(options) # verify if this installation has the iris landmarks
    return detector

# Store image using OpenCV, apply face mesh to image, obtain normalized coordinates (array), save the coordinates to a numpy array
def extract_landmarks(img_path, face_mesh):
    image = mp.Image.create_from_file(str(img_path))
    if image is None:
        print(f"Unable to read image at {img_path}")
        return None
    
    results = face_mesh.detect(image)

    if not results.face_landmarks:
        print(f"Face detection failed for image at {img_path}!")
        return None

    face_landmarks = results.face_landmarks[0]
    face_landmark_coords = [[lm.x, lm.y, lm.z] for lm in face_landmarks] # this list comprehension iterates through all 478 landmarks and saves them to a list
    return np.array(face_landmark_coords, dtype=np.float32) # a numpy array of shape (478, 3)

def generate_dataset_npz(synthetic_dataset_path, output_npz_path):
    # Load the image files in /home/alexis/Desktop/synthetic-dataset/ComfyUI/output/cfd_target/
        # Odd numbers (starting at result_00001) are normal faces
        # Even numbers (starting at result_00002) altered  faces, same person as normal
    
    face_mesh = init_mp()
    normal = []
    altered = []
    dir_path = Path(synthetic_dataset_path)
    num_undetected_faces = 0

    print(f"Extracting landmarks from {synthetic_dataset_path}")
    for image_path in dir_path.iterdir():
        match = re.search(r'\d+', image_path.name)
        if not match:
            continue

        file_index = int(match.group(0))

        try:
            landmark_coords = extract_landmarks(image_path, face_mesh)
            if landmark_coords is None:
                print(f"Landmarks not extracted for {image_path}")
                num_undetected_faces += 1
                continue

            if file_index % 2 == 1:
                normal.append(landmark_coords)
            else:
                altered.append(landmark_coords)
        except Exception as e:
            print(f"Error processing {image_path.name}: {e}")

    face_mesh.close()
    print(num_undetected_faces)

    normal_batched = np.stack(normal, axis=0) if normal else np.empty((0, 478, 3), dtype=np.float32)
    altered_batched = np.stack(altered, axis=0) if altered else np.empty((0, 478, 3), dtype=np.float32)

    print(f"Normal landmarks shape: {normal_batched.shape}")
    print(f"Altered landmarks shape: {altered_batched.shape}")
    print(f"Total undetected faces: {num_undetected_faces}")

    np.savez_compressed(
        output_npz_path,
        normal=normal_batched,
        altered=altered_batched
    )
    print(f"Saved landmarks as .npz to {output_npz_path}")
    

def generate_dataset_pt():
    pass

synthetic_dataset_path = "/home/alexis/Desktop/synthetic-dataset/ComfyUI/output/cfd_target/"
output_path = "/home/alexis/Desktop/landmarks.npz"

#generate_dataset_npz(synthetic_dataset_path, output_path)

# extract_landmarks("/home/alexis/Desktop/synthetic-dataset/ComfyUI/output/cfd_target/result_00563_.png", face_mesh=init_mp())


# you should keep all of this logic in this file, and only load the .pt in the gcn.py file. no point in creating a seperate file for it.
# also note: you should use mediapipe's provided face mesh tesselation (edges/connections between the landmarks) or wireframe.
# this is because using k-nearest neighbours will build the vertex connections based on pure physical proximity, wheras vertex connections are based on anatomy
# if mediapipe's tesselation is used:
# tesselation=vision.FaceLandmarksConnections.FACE_LANDMARKS_TESSELATION



