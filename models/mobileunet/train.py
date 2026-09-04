import math
from pathlib import Path
import torch
import torch.nn.functional as F
from tqdm import tqdm

from dataset import get_dataloaders
from loss_formulation import CompositeLoss
from unet import MobileNetV4UNet, export_onnx

# Computes mean PSNR across the batch; for validation split.
def compute_psnr(pred: torch.Tensor, target: torch.Tensor, eps: float = 1e-10) -> float:
    pred_clamped = pred.clamp(0.0, 1.0)
    mse_per_sample = F.mse_loss(pred_clamped, target, reduction="none").mean(dim=[1, 2, 3])
    psnr_per_sample = torch.where(
        mse_per_sample < eps,
        torch.tensor(100.0, device=pred.device),
        10.0 * torch.log10(1.0 / (mse_per_sample + eps)),
    )
    return psnr_per_sample.mean().item()


def train(
    data_dir: str = "/home/alexis/Desktop/synthetic-dataset/triplet_dataset",
    output_dir: str = "runs",
    epochs: int = 60,
    batch_size: int = 16,
    num_workers: int = 4,
    lr: float = 3e-4,
) -> None:
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    out_dir = Path(output_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    print(f"Using device: {device}")
    train_loader, val_loader = get_dataloaders(
        root=data_dir, batch_size=batch_size, num_workers=num_workers
    )

    model = MobileNetV4UNet(pretrained=True).to(device)
    criterion = CompositeLoss().to(device)
    optimizer = torch.optim.AdamW(model.param_groups(decoder_lr=lr, encoder_lr=lr * 0.1), weight_decay=1e-4)
    scheduler = torch.optim.lr_scheduler.CosineAnnealingLR(optimizer, T_max=epochs, eta_min=1e-6)
    scaler = torch.amp.GradScaler("cuda", enabled=(device.type == "cuda"))

    best_val_loss = float("inf")

    for epoch in range(1, epochs + 1):
        model.train()
        train_loss = 0.0
        train_components = {"charbonnier": 0.0, "sobel": 0.0, "perceptual": 0.0}

        pbar = tqdm(train_loader, desc=f"Epoch [{epoch:03d}/{epochs:03d}]", leave=False)
        for batch in pbar:
            x = batch["input"].to(device, non_blocking=True)
            target = batch["target"].to(device, non_blocking=True)

            optimizer.zero_grad(set_to_none=True)
            with torch.amp.autocast(device_type=device.type, enabled=(device.type == "cuda")):
                _, pred = model(x)
                loss, loss_dict = criterion(pred, target)

            scaler.scale(loss).backward()
            scaler.unscale_(optimizer)
            torch.nn.utils.clip_grad_norm_(model.parameters(), 1.0)
            scaler.step(optimizer)
            scaler.update()

            train_loss += loss.item()
            for k in train_components:
                if k in loss_dict:
                    train_components[k] += loss_dict[k].item()

            pbar.set_postfix(loss=f"{loss.item():.4f}")

        scheduler.step()
        train_loss /= len(train_loader)
        for k in train_components:
            train_components[k] /= len(train_loader)

        # Validation with PSNR
        model.eval()
        val_loss, val_psnr, baseline_psnr = 0.0, 0.0, 0.0
        total_val_samples = 0

        with torch.no_grad():
            for batch in val_loader:
                x = batch["input"].to(device, non_blocking=True)
                warp = batch["warp"].to(device, non_blocking=True)
                target = batch["target"].to(device, non_blocking=True)
                bs = x.size(0)

                with torch.amp.autocast(device_type=device.type, enabled=(device.type == "cuda")):
                    _, pred = model(x)
                    loss, _ = criterion(pred, target)

                val_loss += loss.item() * bs
                val_psnr += compute_psnr(pred, target) * bs
                baseline_psnr += compute_psnr(warp, target) * bs
                total_val_samples += bs

        val_loss /= total_val_samples
        val_psnr /= total_val_samples
        baseline_psnr /= total_val_samples
        psnr_gain = val_psnr - baseline_psnr

        print(
            f"Epoch [{epoch:03d}/{epochs:03d}] | "
            f"Train: {train_loss:.4f} (Charb: {train_components['charbonnier']:.3f}, Sobel: {train_components['sobel']:.3f}, Perc: {train_components['perceptual']:.3f}) | "
            f"Val: {val_loss:.4f} | "
            f"Val PSNR: {val_psnr:.2f} dB (Warp: {baseline_psnr:.2f} dB, Gain: {psnr_gain:+.2f} dB)"
        )
        if val_loss < best_val_loss:
            best_val_loss = val_loss
            torch.save(model.state_dict(), out_dir / "best_model.pt")

    # Export best checkpoint to ONNX
    best_weights_path = out_dir / "best_model.pt"
    model.load_state_dict(torch.load(best_weights_path, map_location="cpu", weights_only=True))
    export_onnx(model, "/home/alexis/git/stroke-cv-cpp/flutter_ui/assets/mobileunet.onnx")
    print("Successfully exported mobileunet.onnx to flutter assets directory.")


if __name__ == "__main__":
    train()