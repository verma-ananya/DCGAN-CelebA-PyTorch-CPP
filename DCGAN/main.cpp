//
//  main.cpp
//  DCGAN
//
//  Created by Kushashwa Ravi Shrimali on 19/08/19.
//  Copyright © 2019 Kushashwa Ravi Shrimali. All rights reserved.
//

#include <iostream>
#include <torch/torch.h>
#include "network.hpp"
#include "dataset.hpp"

class Arguments {
public:
    std::string dataroot;
    int workers;
    int batch_size;
    int image_size;
    int nc;
    int nz;
    int ngf;
    int ndf;
    int num_epochs;
    float lr;
    float beta1;
    int ngpu;
    Arguments(std::string data_root, int num_workers, int bs, int img_size, int num_channels, int length_latent_vector, int depth_feature_maps_g, int depth_feature_maps_d, int number_epochs, float learning_rate, float beta_1, int num_gpu) {
        dataroot = data_root;
        workers = num_workers;
        batch_size = bs;
        image_size = img_size;
        nc = num_channels;
        nz = length_latent_vector;
        ngf = depth_feature_maps_g;
        ndf = depth_feature_maps_d;
        num_epochs = number_epochs;
        lr = learning_rate;
        beta1 = beta_1;
        ngpu = num_gpu;
    };
};

int main(int argc, const char * argv[]) {
    Arguments args = Arguments("data/celeba", 2, 128, 64, 3, 100, 64, 64, 5, 0.0002, 0.5, 1);
    std::cout << args.batch_size << std::endl;
    std::cout << "Data Root: " << args.dataroot;
    std::string images_name = args.dataroot + "/img_align_celeba";
//    std::string dogs_name = args.dataroot + "/dog_test";
    
    std::vector<std::string> folders_name;
    folders_name.push_back(images_name);
//    folders_name.push_back(dogs_name);
    
    // Get paths of images and labels as int from the folder paths
    std::pair<std::vector<std::string>, std::vector<int>> pair_images_labels = load_data_from_folder(folders_name);
    
    std::vector<std::string> list_images = pair_images_labels.first;
    std::vector<int> list_labels = pair_images_labels.second;
    
    auto custom_dataset = CustomDataset(list_images, list_labels, 224).map(torch::data::transforms::Normalize<>(0.5, 0.5)).map(torch::data::transforms::Stack<>());
    
    auto data_loader = torch::data::make_data_loader<torch::data::samplers::RandomSampler>(std::move(custom_dataset), args.batch_size);
    
    torch::Device device = torch::kCPU;
    if(torch::cuda::is_available()) {
        device = torch::kCUDA;
    }
    
    Generator g = Generator();
    Discriminator d = Discriminator();
    
    torch::nn::Sequential netG = g.main_func();
    torch::nn::Sequential netD = d.main_func();
    netG->to(device);
    netD->to(device);
    
    torch::optim::Adam optimizerG(
                                           netG->parameters(), torch::optim::AdamOptions(2e-4).beta1(0.5));
    torch::optim::Adam optimizerD(
                                               netD->parameters(), torch::optim::AdamOptions(5e-4).beta1(0.5));
    
    for(int64_t epoch=1; epoch<=10; ++epoch) {
        for(torch::data::Example<>& batch: *data_loader) {
            netD->zero_grad();
            torch::Tensor real_images = batch.data;
            torch::Tensor real_labels = torch::empty(batch.data.size(0)).uniform_(0.8, 1.0);
            torch::Tensor real_output = netD->forward(real_images);
            torch::Tensor d_loss_real = torch::binary_cross_entropy(real_output, real_labels);
            d_loss_real.backward();
            
            // Train discriminator with fake images
            torch::Tensor noise = torch::randn({batch.data.size(0), args.nz, 1, 1});
            torch::Tensor fake_images = netG->forward(noise);
            torch::Tensor fake_labels = torch::zeros(batch.data.size(0));
            torch::Tensor fake_output = netD->forward(fake_images.detach());
            torch::Tensor d_loss_fake = torch::binary_cross_entropy(fake_output, fake_labels);
            d_loss_fake.backward();
            
            torch::Tensor d_loss = d_loss_real + d_loss_fake;
            optimizerD.step();
            
            // Train generator
            netG->zero_grad();
            fake_labels.fill_(1);
            fake_output = netD->forward(fake_images);
            torch::Tensor g_loss = torch::binary_cross_entropy(fake_output, fake_labels);
            g_loss.backward();
            optimizerG.step();
            
            std::cout << "Epoch: " << epoch << ", D_loss: " << d_loss.item<float>() << ", G_loss: " << g_loss.item<float>() << std::endl;
        }
    }
    return 0;
}
