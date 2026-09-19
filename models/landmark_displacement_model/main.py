import torch
import torch.nn.functional as F
from torch_geometric.utils import to_dense_adj
from gcn import LandmarkDisplacementModel
import onnx
import onnxscript

FACE_OVAL_INDICES = [
    10, 338, 297, 332, 284, 251, 21, 54, 103, 67, 109
]

EYE_INDICES = [
    33, 7, 163, 144, 145, 153, 154, 155, 133, 246, 161, 160, 159, 158, 157, 173, 468, 469, 470, 471, 472,
    263, 249, 390, 373, 374, 380, 381, 382, 362, 466, 388, 387, 386, 385, 384, 398, 473, 474, 475, 476, 477
]

# Landmarks more commonly percived to be associated with right-sided stroke
ORAL_COMMISSURE_VERMILION = [291, 375, 321, 405, 314, 17, 84, 181, 91]   # mouth opening, corner drop, lower lip sag
NASOLABIAL_MIDFACE = [391, 322, 410, 432, 287, 436]                      # nesolabial fold
CHEEK_BUCCINATOR = [361, 323, 366, 447, 345]                             # cheek
LOWER_MANDIBULAR_JOWL = [377, 400, 378, 379, 365, 397]                   # tissue droop at the jawline

RIGHT_STROKE_ZONE = sorted(list(set(
    ORAL_COMMISSURE_VERMILION + 
    NASOLABIAL_MIDFACE + 
    CHEEK_BUCCINATOR + 
    LOWER_MANDIBULAR_JOWL
)))

def main():
    device = torch.device('cuda' if torch.cuda.is_available() else 'cpu')
    dataset_path = "/home/alexis/Desktop/landmarks_dataset.pt"
    data, slices = torch.load(dataset_path, weights_only=False)
    edge_index = data.edge_index
    adj_matrix = to_dense_adj(edge_index)[0].to(device)
    adj_matrix = adj_matrix + torch.eye(478, device=device)
    num_samples = len(slices['x']) - 1 

    inputs = data.x.view(num_samples, 478, 3).to(device)
    targets = data.y.view(num_samples, 478, 3).to(device)

    # To penalize the model from moving face mesh indices that are at the border of the face or eyes
    boundary_indices = torch.tensor(FACE_OVAL_INDICES, dtype=torch.long, device=device)
    eye_indices = torch.tensor(EYE_INDICES, dtype=torch.long, device=device)
    lambda_boundary = 0.50
    lambda_eye = 0.50

    # Weighting areas 'more likely' to be implicated in / percieved in stroke more higher then others.
    loss_weights = torch.ones(478, device=device)
    stroke_indices = torch.tensor(RIGHT_STROKE_ZONE, dtype=torch.long, device=device)
    loss_weights[stroke_indices] = 5.0
    loss_weights = loss_weights.view(1, 478, 1)

    model = LandmarkDisplacementModel(adjacency_matrix=adj_matrix, hidden_dimension=128).to(device)
    optimizer = torch.optim.AdamW(model.parameters(), lr=5e-4, weight_decay=1e-4)
    scheduler = torch.optim.lr_scheduler.CosineAnnealingLR(optimizer, T_max=5000, eta_min=1e-6)

    print(f"Training DenseGCN on {device} ({num_samples} samples)")

    model.train()

    for epoch in range(1, 5001):
        optimizer.zero_grad()
        _, pred_delta = model(inputs)
        huber_raw = F.huber_loss(pred_delta, targets, delta=0.08, reduction='none')
        huber_loss = (huber_raw * loss_weights).sum() / (loss_weights.sum() * num_samples * 3)
        boundary_penalty = lambda_boundary * (pred_delta[:, boundary_indices, :] ** 2).mean()
        eye_penalty = lambda_eye * (pred_delta[:, eye_indices, :] ** 2).mean()
        loss = huber_loss + boundary_penalty + eye_penalty
        loss.backward()
        optimizer.step()
        scheduler.step()
        if epoch % 50 == 0 or epoch == 1:
            print(f"Epoch {epoch:03d} | Total Loss: {loss.item():.6f} (Huber: {huber_loss.item():.6f}, Eye Penalty: {eye_penalty.item():.6f})")

    model.eval()
    example_input = torch.randn(1, 478, 3, device=device)
    onnx_path = "/home/alexis/git/stroke-cv-cpp/flutter_ui/assets/landmark_displacement_model.onnx"
    torch.onnx.export(
        model,
        example_input, 
        onnx_path,
        export_params=True,
        opset_version=16,
        do_constant_folding=True,
        input_names=['input_landmarks'],
        output_names=['transformed_landmarks', 'displacement_deltas'],
        dynamic_axes={
            'input_landmarks': {0: 'batch_size'},
            'transformed_landmarks': {0: 'batch_size'},
            'displacement_deltas': {0: 'batch_size'}
        }
    )
    model_proto = onnx.load(onnx_path)
    onnx.save_model(model_proto, onnx_path, save_as_external_data=False)

    print(f"Saved ONNX model to {onnx_path}")


if __name__ == "__main__":
    main()
