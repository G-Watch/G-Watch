import time
import json
import torch
import torch.nn as nn
import torch.optim as optim
import torch.nn.functional as F
import numpy as np
import tqdm

import gwatch.cuda.profile as gwatch_profile


torch.manual_seed(0)
def generate_random_data(m, n, k):
    X = np.random.rand(m, n).astype(np.float32)
    logits = np.random.rand(m, k).astype(np.float32)
    y = torch.softmax(torch.tensor(logits), dim=1)
    return torch.tensor(X), y


class LargeNN(nn.Module):
    def __init__(self, in_size : int, embedding_size : int, out_size : int):
        super(LargeNN, self).__init__()
        self.fc1 = nn.Linear(in_size, embedding_size)
        self.fc2 = nn.Linear(embedding_size, embedding_size)
        self.fc3 = nn.Linear(embedding_size, embedding_size)
        self.fc4= nn.Linear(embedding_size, out_size)

        # create a profile context and profiler
        self.gw_profile_context = gwatch_profile.ProfileContext()
        self.gw_profiler = self.gw_profile_context.create_profiler(
            device_id,
            [
                "dram__bytes_read.sum.per_second"
            ]
        )


    def forward(self, x):
        with self.gw_profiler.RangeProfile(range_name="forward") as p:
            x = self.fc1(x)
            x = F.relu(x)
            x = self.fc2(x)
            x = F.relu(x)
            x = self.fc3(x)
            x = F.relu(x)
            x = self.fc4(x)
        print(p.metrics)
        return x


batch_size = 64
in_size = 256
out_size = 16
embedding_size = 1024
num_epochs = 1
num_samples = batch_size * 32
learning_rate = 0.01
device_id = 0


model = LargeNN(in_size=in_size, embedding_size=embedding_size, out_size=out_size).to(f'cuda:{device_id}')
criterion = nn.MSELoss()
optimizer = optim.SGD(model.parameters(), lr=learning_rate)

Xs = []
Ys = []
for i in range(int(num_samples/batch_size)):
    X, Y = generate_random_data(m=batch_size, n=in_size, k=out_size)
    Xs.append(X)
    Ys.append(Y)


# start training
for epoch in tqdm.tqdm(range(num_epochs)):
    for iter in range(int(num_samples/batch_size)):
        X = Xs[iter].to(f"cuda:{device_id}")
        Y = Ys[iter].to(f"cuda:{device_id}")
        model.train()
        optimizer.zero_grad()
        outputs = model(X)
        loss = criterion(outputs, Y)
        loss.backward()
        optimizer.step()
        break
