import torch
import torch.nn.functional as F
from torch_geometric.utils import to_dense_adj
from gcn import LandmarkDisplacementModel
import onnx
import onnxscript

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

    model = LandmarkDisplacementModel(adjacency_matrix=adj_matrix, hidden_dimension=128).to(device)
    optimizer = torch.optim.Adam(model.parameters(), lr=0.001)

    print(f"Training DenseGCN on {device} ({num_samples} samples)")

    model.train()

    for epoch in range(1, 10001):
        optimizer.zero_grad()
        _, pred_delta = model(inputs)
        loss = F.huber_loss(pred_delta, targets, delta=1.0)
        loss.backward()
        optimizer.step()
        if epoch % 50 == 0 or epoch == 1:
            print(f"Epoch {epoch:03d} | Huber Loss: {loss.item():.6f}")

    model.eval()
    example_input = torch.randn(1, 478, 3, device=device)
    onnx_path = "/home/alexis/Desktop/landmark_displacement_model.onnx"
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

    print(f"Saved ONNX model to {onnx_path}")


if __name__ == "__main__":
    main()
