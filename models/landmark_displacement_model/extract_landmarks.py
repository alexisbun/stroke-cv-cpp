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
from mediapipe.tasks.python.vision import FaceLandmarksConnections
import torch
from torch_geometric.data import Data, InMemoryDataset

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
    max_person_id = max([int(re.search(r'\d+', f.name).group(0)) for f in dir_path.glob('*result_*')]) // 2
    for person_id in range(1, max_person_id + 1):
        normal_idx = 2 * person_id - 1 
        altered_idx = 2 * person_id 

        normal_img = list(dir_path.glob(f"*result_{normal_idx:05d}_*"))
        altered_img = list(dir_path.glob(f"*result_{altered_idx:05d}_*"))

        if not normal_img or not altered_img:
            continue

        norm_coords = extract_landmarks(normal_img[0], face_mesh)
        alt_coords = extract_landmarks(altered_img[0], face_mesh)

        if norm_coords is not None and alt_coords is not None:
            normal.append(norm_coords)
            altered.append(alt_coords)
        else:
            print(f"Skipping Person {person_id} due to detection failure.")
            num_undetected_faces += 1

    face_mesh.close()
    print(num_undetected_faces)

    normal_batched = np.stack(normal, axis=0) if normal else np.empty((0, 478, 3), dtype=np.float32)
    altered_batched = np.stack(altered, axis=0) if altered else np.empty((0, 478, 3), dtype=np.float32)

    print(f"Normal landmarks shape: {normal_batched.shape}") # Output: (522, 478, 3)
    print(f"Altered landmarks shape: {altered_batched.shape}") # Output: (522, 478, 3)
    print(f"Total undetected faces: {num_undetected_faces}") # Output: 0

    np.savez_compressed(
        output_npz_path,
        normal=normal_batched,
        altered=altered_batched
    )
    print(f"Saved landmarks as .npz to {output_npz_path}")
    
# construct edge_index attribute for PyTorch graph connectivity
def graph_connectivity():
    connections = FaceLandmarksConnections.FACE_LANDMARKS_TESSELATION
    base_edges = [(c.start, c.end) for c in connections]  # list of tuples representing 468 (iris landmarks not present) MP face landmarker edge connections

    # creating custom edge connections for iris landmarks
    left_eye_contour = [33, 133, 159, 145, 153, 144, 163, 7]
    right_eye_contour = [362, 263, 386, 374, 380, 373, 390, 249]  

    custom_edges = []
    for index in left_eye_contour + [469, 470, 471, 472]:
        custom_edges.extend([(468, index), (index, 468)])
    for index in right_eye_contour + [474, 475, 476, 477]:
        custom_edges.extend([(473, index), (index, 473)])

    all_edges = base_edges + custom_edges
    bidirectional_edges = set()
    for u, v in all_edges:
        bidirectional_edges.add((u, v))
        bidirectional_edges.add((v, u))

    edge_index = torch.tensor(list(bidirectional_edges), dtype=torch.long).t().contiguous()
    return edge_index

# Covert the .npz to a PyTorch geometric graph (.pt)
def generate_dataset_pt(npz_path="/home/alexis/Desktop/landmarks.npz", save_path="/home/alexis/Desktop/landmarks_dataset.pt"):
    data_npz = np.load(npz_path)
    normal_coords = data_npz['normal']
    altered_coords = data_npz['altered']

    edge_index = graph_connectivity()

    # coordinate normalization
    data_list = []
    for i in range(len(normal_coords)):
        norm_raw = torch.tensor(normal_coords[i], dtype=torch.float)
        alt_raw = torch.tensor(altered_coords[i], dtype=torch.float)

        center = norm_raw[4:5, :]

        scale = torch.norm(norm_raw[468] - norm_raw[473], p=2)

        x_norm = (norm_raw - center)/scale
        alt_norm = (alt_raw - center)/scale

        y_displacement = alt_norm - x_norm

        graph = Data(
            x=x_norm,
            edge_index=edge_index,
            y=y_displacement,
            scale=scale,
            center=center,
        )
        data_list.append(graph)

    data, slices = InMemoryDataset.collate(data_list)
    torch.save((data,slices), save_path)

    print(f"Successfully generated PyTorch Geometric dataset with {len(data_list)} samples as '.pt' to {save_path}.")

    
synthetic_dataset_path = "/home/alexis/Desktop/synthetic-dataset/ComfyUI/output/cfd_target/"
output_path = "/home/alexis/Desktop/landmarks.npz"

# generate_dataset_npz(synthetic_dataset_path, output_path)
generate_dataset_pt()
# extract_landmarks("/home/alexis/Desktop/synthetic-dataset/ComfyUI/output/cfd_target/result_00563_.png", face_mesh=init_mp())

