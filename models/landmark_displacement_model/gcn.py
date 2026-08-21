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
        self.conv3 = DenseGCNConv(hidden_dimension, 3)

    def forward(self, pos):
        h = F.relu(self.conv1(pos, self.adj))
        h = F.relu(self.conv2(h, self.adj))
        delta_x = self.conv3(h, self.adj)

        return pos + delta_x, delta_x