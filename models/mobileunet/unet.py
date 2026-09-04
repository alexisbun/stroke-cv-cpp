# UNet using a pretrained MobileNetV4 encoder and decoder using depthwise-seperable convolutions.
import torch
import torch.nn as nn
import timm

ENCODER_NAME = "mobilenetv4_conv_small.e2400_r224_in1k"

# default mean and standard deviation for red, green, blue color channels for the imagenet dataset
IMAGENET_MEAN = (0.485, 0.456, 0.406) # https://github.com/huggingface/pytorch-image-models/blob/main/timm/data/constants.py
IMAGENET_STD = (0.229, 0.224, 0.225)  

# Depthwise convolution followed by pointwise convolution, each with batch normalization and a ReLU6 activation.
class DepthwiseSeparableConv(nn.Module):
    def __init__(self, in_channels, out_channels):
        super().__init__()
        self.ds = nn.Sequential(
            # Depthwise
            nn.Conv2d(in_channels, in_channels, kernel_size=3, padding=1, groups=in_channels, bias=False),
            nn.BatchNorm2d(in_channels),
            nn.ReLU6(inplace=True),
            # Pointwise 
            nn.Conv2d(in_channels, out_channels, kernel_size=1, bias=False),
            nn.BatchNorm2d(out_channels),
            nn.ReLU6(inplace=True),
        )
    def forward(self, x):
        return self.ds(x)

# Upsample by 2, merge encoder feature maps with decoder's using skip connections, and apply a 3x3 convolution to refine the output.
class Decoder(nn.Module):
    def __init__(self, in_channels, skip_channels, out_channels):
        super().__init__()
        self.upsample = nn.Upsample(scale_factor=2, mode="bilinear", align_corners=False)
        self.skip_connection = nn.Sequential(
            nn.Conv2d(in_channels + skip_channels, out_channels, kernel_size=1, bias=False),
            nn.BatchNorm2d(out_channels),
            nn.ReLU6(inplace=True),
        )
        self.residual = DepthwiseSeparableConv(out_channels, out_channels) # refine after merging encoder-decoder with 3x3 convolution
    def forward(self, x, skip):
        x = self.skip_connection(torch.cat([self.upsample(x), skip], dim=1)) 
        return x + self.residual(x)


class MobileNetV4UNet(nn.Module):
    def __init__(
        self,
        decoder_channels=(96, 64, 48, 32), # can change to (64, 48, 32, 16) for lower latency, can compare differences
        pretrained=True,       
        drop_path_rate=0.0
    ):
        super().__init__()
        self.encoder = timm.create_model(
            ENCODER_NAME,
            pretrained=pretrained,
            features_only=True,
            in_chans=6,
            drop_path_rate=drop_path_rate
        )
        enc = list(self.encoder.feature_info.channels())  # [32, 32, 64, 96, 960]

        # Set the 4th block of the encoder layer to a identity function (effectivly deleting the 4th encoder block, 
        # which previously would expand the number of channels to 960 and would be computationally expensive).
        self.encoder.blocks[4] = nn.Identity() 
        enc[4] = 128

        # register_buffer saves mean and std (not as parameters) inside of the module's state (so they move to the GPU and save with model).
        self.register_buffer("mean", torch.tensor(IMAGENET_MEAN * 2).view(1, 6, 1, 1))
        self.register_buffer("std", torch.tensor(IMAGENET_STD * 2).view(1, 6, 1, 1))
        
        d = decoder_channels                       # num channels: 
        self.dec4 = Decoder(enc[4], enc[3], d[0])  # 96
        self.dec3 = Decoder(d[0], enc[2], d[1])    # 64 
        self.dec2 = Decoder(d[1], enc[1], d[2])    # 48
        self.dec1 = Decoder(d[2], enc[0], d[3])    # 32

        # Upsample to original input resolution (256x256)
        self.up = nn.Upsample(scale_factor=2, mode="bilinear", align_corners=False) 
        
        self.head = nn.Conv2d(d[3], 3, kernel_size=1) # Final convolution mapping 32 feature maps to 3 RGB channels (which is the residual delta map).
        nn.init.zeros_(self.head.weight)
        nn.init.zeros_(self.head.bias)

    def forward(self, x):
        i_warp = x[:, 3:] # Channels 3, 4, 5 representing the warped image

        # Normalize and extract feature maps from the encoder.
        c1, c2, c3, c4, c5 = self.encoder((x - self.mean) / self.std)

        # Upscale feature maps and combine them with encoder's features via skip connections
        y = self.dec4(c5, c4)
        y = self.dec3(y, c3)
        y = self.dec2(y, c2)
        y = self.dec1(y, c1)
        y = self.up(y)
    
        h = self.head(y)
        delta = torch.tanh(h) # Force model outputs to [-1, 1]
    
        return delta, i_warp + delta

    def train(self, mode=True):
        super().train(mode)
        if mode:
            for m in self.encoder.modules():
                if isinstance(m, nn.modules.batchnorm._BatchNorm): 
                    m.eval() # Use fixed BatchNorm statistics (they aren't updated during training, since they're pre-calculated)
        return self

    def param_groups(self, decoder_lr=3e-4, encoder_lr=3e-5):
        # Assign different learning rates for encoder and decoder
        enc = [p for n, p in self.named_parameters() if n.startswith("encoder.") and p.requires_grad]
        dec = [p for n, p in self.named_parameters() if not n.startswith("encoder.") and p.requires_grad]
        return [{"params": dec, "lr": decoder_lr}, {"params": enc, "lr": encoder_lr}]
    

class ExportWrapper(nn.Module):
    def __init__(self, model):
        super().__init__()
        self.model = model
    def forward(self, x):
        return self.model(x)[0]
    
def export_onnx(model, path):
    was_training = model.training
    orig_device = next(model.parameters()).device
    model_cpu = model.cpu().eval()
    try:
        dummy_input = torch.zeros(1, 6, 256, 256, device="cpu")
        torch.onnx.export(
            ExportWrapper(model_cpu),
            (dummy_input,),
            str(path),
            opset_version=16,
            do_constant_folding=True,
            input_names=["input"],
            output_names=["delta"],
            dynamic_axes={
                "input": {0: "batch"},
                "delta": {0: "batch"}
            },
            dynamo=False,
        )
    finally:
        model.to(orig_device)
        model.train(was_training)