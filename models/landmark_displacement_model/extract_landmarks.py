import numpy as np
import mediapipe as mp
import cv2

def init_mp():
    pass

# Goal: Given an image of a face, use MediaPipe face mesh to extract the 478 landmarks. Apply for all faces. Store results in a file.
def extract_landmarks(img_path):
    pass
    # 1. Initialize mediapipe face mesh model (decide if using the tasks or solutions api's. i think the tasks since thats my what i already implement.)
    # 1.1 Do in seperate function since we are in a method for extracting the landmarks of one image
    # 2. Store image using OpenCV.
    # 3. Apply face mesh to image.
    # 4. Obtain normalized coordinates (array)
    # 5. Save the coordinates to a numpy array

# inside a try catch block (so I can see if unfortunatly any of the low res faces weren't detected)
# loop over all files in a folder:
    # call extract_landmarks()
    # send the data to some data structure (tuple?)

# save the data structure to a file: 
# .pt if you dont care about caching the coordinate data, and just want to save it as a graph.
# .npz to keep the raw coordinate outputs as a seperate file for whatever reason, just make sure to convert to a .pt still when you run the model training script.
