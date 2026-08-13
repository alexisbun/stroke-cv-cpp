import torch
import torch.nn as nn
import torch.nn.functional as F
from torch_geometric.nn import DenseGCNConv

class LandmarkDisplacementModel(nn.Module):
    def __init__(self, adjacency_matrix, hidden_dimension=128):
        super().__init__()
        self.register_buffer('adj', adjacency_matrix)
        self.conv1 = DenseGCNConv(3, hidden_dimension)
        self.conv2 = DenseGCNConv(hidden_dimension, hidden_dimension)
        self.fc1 = nn.Linear(hidden_dimension, hidden_dimension)
        self.fc2 = nn.Linear(hidden_dimension, 3)

    def forward(self, pos):
        h1 = F.relu(self.conv1(pos, self.adj))
        h2 = F.relu(self.conv2(h1, self.adj)) + h1
        h3 = F.relu(self.fc1(h2))
        delta_x = self.fc2(h3)

        return pos + delta_x, delta_x