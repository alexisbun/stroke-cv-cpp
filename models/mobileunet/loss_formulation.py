import torch
import torch.nn as nn
import torch.nn.functional as F
import timm

IMAGENET_MEAN = (0.485, 0.456, 0.406)
IMAGENET_STD = (0.229, 0.224, 0.225)

# Computes mean Charbonnier loss: L = mean(sqrt((pred - target)^2 + eps^2)). eps = epsilon.
class CharbonnierLoss(nn.Module):
    def __init__(self, eps: float = 1e-3):
        super().__init__()
        self.eps_sq = eps ** 2

    def forward(self, pred: torch.Tensor, target: torch.Tensor) -> torch.Tensor:
        return torch.mean(torch.sqrt((pred - target) ** 2 + self.eps_sq))

# Computes the Euclidean edge gradient magnitude between model prediction and target to penalize blurry edges.
class SobelLoss(nn.Module):
    def __init__(self, eps: float = 1e-6):
        super().__init__()
        self.eps = eps

        # Horizontal and vertical Sobel filters
        kx = torch.tensor([[-1.0, 0.0, 1.0], [-2.0, 0.0, 2.0], [-1.0, 0.0, 1.0]]).view(1, 1, 3, 3)
        ky = torch.tensor([[-1.0, -2.0, -1.0], [0.0, 0.0, 0.0], [1.0,  2.0, 1.0]]).view(1, 1, 3, 3)
        self.register_buffer("kx", kx.repeat(3, 1, 1, 1))
        self.register_buffer("ky", ky.repeat(3, 1, 1, 1))

    def compute_magnitude(self, x: torch.Tensor) -> torch.Tensor:
        x = F.pad(x, (1, 1, 1, 1), mode="replicate")
        gx = F.conv2d(x, self.kx, groups=3)
        gy = F.conv2d(x, self.ky, groups=3)
        return torch.sqrt(gx ** 2 + gy ** 2 + self.eps)

    def forward(self, pred: torch.Tensor, target: torch.Tensor) -> torch.Tensor:
        return F.l1_loss(self.compute_magnitude(pred), self.compute_magnitude(target))

# Use a pretrained VGG-16 model to compute perceptual loss between prediction and target to preserve natural skin texture and the identity of the person.
class PerceptualLoss(nn.Module):
    def __init__(self):
        super().__init__()
        self.vgg = timm.create_model(
            "vgg16.tv_in1k", pretrained=True, features_only=True, out_indices=(1,)
        ) # Extract features only at features.8, i.e. relu2_2 (relu2_2 are features after the 2nd conv in block 2 of vgg16) 
        self.vgg.eval()
        for p in self.vgg.parameters():
            p.requires_grad_(False)

        self.register_buffer("mean", torch.tensor(IMAGENET_MEAN).view(1, 3, 1, 1))
        self.register_buffer("std", torch.tensor(IMAGENET_STD).view(1, 3, 1, 1))

    def train(self, mode: bool = True):
        super().train(mode)
        self.vgg.eval()
        return self

    def extract_features(self, x: torch.Tensor) -> torch.Tensor:
        return self.vgg((x - self.mean) / self.std)[0]

    def forward(self, pred: torch.Tensor, target: torch.Tensor) -> torch.Tensor:
        feat_pred = self.extract_features(pred)
        with torch.no_grad():
            feat_target = self.extract_features(target)
        return F.mse_loss(feat_pred, feat_target)


class CompositeLoss(nn.Module):
    def __init__(self, w_char=1.0, w_perc=0.003, w_sobel=0.20):
        super().__init__()
        self.w_char = w_char
        self.w_perc = w_perc
        self.w_sobel = w_sobel

        self.charbonnier = CharbonnierLoss()
        self.sobel = SobelLoss()
        self.perceptual = PerceptualLoss() if w_perc > 0 else None

    def forward(
        self,
        pred: torch.Tensor,
        target: torch.Tensor,
    ):
        loss_components = {
            "charbonnier": self.charbonnier(pred, target),
            "sobel": self.sobel(pred, target),
        }
        total = self.w_char * loss_components["charbonnier"] + self.w_sobel * loss_components["sobel"]

        if self.perceptual is not None:
            loss_components["perceptual"] = self.perceptual(pred, target)
            total = total + self.w_perc * loss_components["perceptual"]


        return total, {k: v.detach() for k, v in loss_components.items()}
