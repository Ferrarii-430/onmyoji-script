#include "YOLODetector.h"

#include <numeric>

bool YOLODetector::initialize(const std::wstring& model_path) {
    if (is_initialized) {
        std::cout << "YOLODetector already initialized!" << std::endl;
        return true;
    }

    try {
        env = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "YOLODetector");

        Ort::SessionOptions session_options;
        session_options.SetIntraOpNumThreads(1);
        session_options.SetInterOpNumThreads(1);

        session = std::make_unique<Ort::Session>(*env, model_path.c_str(), session_options);

        Ort::AllocatorWithDefaultOptions allocator;

        auto input_name_ptr = session->GetInputNameAllocated(0, allocator);
        input_name = input_name_ptr.get();
        auto input_type_info = session->GetInputTypeInfo(0);
        auto input_tensor_info = input_type_info.GetTensorTypeAndShapeInfo();
        input_shape = input_tensor_info.GetShape();

        auto output_name_ptr = session->GetOutputNameAllocated(0, allocator);
        output_name = output_name_ptr.get();
        auto output_type_info = session->GetOutputTypeInfo(0);
        auto output_tensor_info = output_type_info.GetTensorTypeAndShapeInfo();
        output_shape = output_tensor_info.GetShape();

        std::cout << "Model loaded successfully. Input: " << input_name
                  << ", Output: " << output_name << std::endl;

        is_initialized = true;
        return true;

    } catch (const Ort::Exception& e) {
        std::cerr << "ONNX Runtime error in initialization: " << e.what() << std::endl;
        return false;
    } catch (const std::exception& e) {
        std::cerr << "Standard exception in initialization: " << e.what() << std::endl;
        return false;
    }
}

// 检测方法
std::vector<Detection> YOLODetector::detect(const cv::Mat& image, double nms_threshold) {
    std::vector<Detection> final_detections;

    if (!is_initialized) {
        std::cerr << "YOLODetector not initialized! Call initialize() first." << std::endl;
        return final_detections;
    }

    std::lock_guard<std::mutex> lock(detect_mutex);  // 确保线程安全

    try {
        if (image.empty()) {
            std::cout << "Input image is empty!" << std::endl;
            return final_detections;
        }

        // 图像预处理参数
        const int input_width = 640;
        const int input_height = 640;
        const float confidence_threshold = 0.4f;
        // const float nms_threshold = 0.55f;

        cv::Mat original_image = image.clone();
        cv::Mat resized_image, float_image;

        // 保持宽高比的Resize
        float scale = std::min(static_cast<float>(input_width) / image.cols,
                              static_cast<float>(input_height) / image.rows);
        int new_width = static_cast<int>(image.cols * scale);
        int new_height = static_cast<int>(image.rows * scale);
        cv::resize(image, resized_image, cv::Size(new_width, new_height));

        // 填充图像
        cv::Mat padded_image = cv::Mat::zeros(input_height, input_width, CV_8UC3);
        cv::Scalar padding_color(114, 114, 114);
        int dx = (input_width - new_width) / 2;
        int dy = (input_height - new_height) / 2;
        resized_image.copyTo(padded_image(cv::Rect(dx, dy, new_width, new_height)));

        // 归一化并转换通道
        padded_image.convertTo(float_image, CV_32F, 1.0 / 255.0);
        cv::cvtColor(float_image, float_image, cv::COLOR_BGR2RGB);

        // HWC -> CHW
        std::vector<float> input_tensor_values(input_width * input_height * 3);
        std::vector<cv::Mat> channels(3);
        cv::split(float_image, channels);
        size_t channel_size = input_width * input_height;
        for (int i = 0; i < 3; ++i) {
            memcpy(input_tensor_values.data() + i * channel_size,
                   channels[i].data, channel_size * sizeof(float));
        }

        // 准备输入张量
        std::vector<int64_t> input_shape_vec = {1, 3, input_height, input_width};
        auto memory_info = Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeCPU);
        Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
            memory_info,
            input_tensor_values.data(),
            input_tensor_values.size(),
            input_shape_vec.data(),
            input_shape_vec.size()
        );

        // 准备输入输出名称
        std::vector<const char*> input_names = {input_name.c_str()};
        std::vector<const char*> output_names = {output_name.c_str()};

        // 运行推理
        auto output_tensors = session->Run(
            Ort::RunOptions{nullptr},
            input_names.data(),
            &input_tensor,
            1,
            output_names.data(),
            1
        );

        // 解析输出
        float* output_data = output_tensors[0].GetTensorMutableData<float>();
        auto output_shape_info = output_tensors[0].GetTensorTypeAndShapeInfo();
        auto output_shape_vec = output_shape_info.GetShape();

        std::vector<Detection> raw_detections;
        if (output_shape_vec.size() == 3 && output_shape_vec[1] == 25200) {
            int num_detections = output_shape_vec[1];
            int num_features = output_shape_vec[2];
            int num_classes = num_features - 5;

            for (int i = 0; i < num_detections; ++i) {
                float* detection = output_data + i * num_features;
                float objectness = detection[4];
                if (objectness > confidence_threshold) {
                    int class_id = std::max_element(detection + 5, detection + 5 + num_classes) - (detection + 5);
                    float class_confidence = detection[5 + class_id];
                    float total_confidence = objectness * class_confidence;
                    if (total_confidence > confidence_threshold) {
                        // 转换坐标到原始图像空间
                        float center_x = detection[0];
                        float center_y = detection[1];
                        float width = detection[2];
                        float height = detection[3];

                        float x1 = (center_x - width / 2 - dx) / scale;
                        float y1 = (center_y - height / 2 - dy) / scale;
                        float x2 = (center_x + width / 2 - dx) / scale;
                        float y2 = (center_y + height / 2 - dy) / scale;

                        x1 = std::max(0.0f, std::min(x1, static_cast<float>(image.cols)));
                        y1 = std::max(0.0f, std::min(y1, static_cast<float>(image.rows)));
                        x2 = std::max(0.0f, std::min(x2, static_cast<float>(image.cols)));
                        y2 = std::max(0.0f, std::min(y2, static_cast<float>(image.rows)));

                        Detection det;
                        det.bbox = cv::Rect(
                            static_cast<int>(x1),
                            static_cast<int>(y1),
                            static_cast<int>(x2 - x1),
                            static_cast<int>(y2 - y1)
                        );
                        det.confidence = total_confidence;
                        det.class_id = class_id;
                        raw_detections.push_back(det);
                    }
                }
            }
        }

        // 应用非极大值抑制
        final_detections = nonMaximumSuppression(raw_detections, nms_threshold);

    } catch (const Ort::Exception& e) {
        std::cerr << "ONNX Runtime error in detect: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Standard exception in detect: " << e.what() << std::endl;
    }

    return final_detections;
}

// 非极大值抑制 (NMS)
std::vector<Detection> YOLODetector::nonMaximumSuppression(const std::vector<Detection>& detections, double iou_threshold) {
    std::vector<Detection> result;
    std::vector<bool> suppressed(detections.size(), false);

    // 按置信度降序排序
    std::vector<size_t> indices(detections.size());
    std::iota(indices.begin(), indices.end(), 0);
    std::sort(indices.begin(), indices.end(), [&](size_t i, size_t j) {
        return detections[i].confidence > detections[j].confidence;
    });

    for (size_t i = 0; i < indices.size(); ++i) {
        if (suppressed[indices[i]]) continue;

        result.push_back(detections[indices[i]]);

        for (size_t j = i + 1; j < indices.size(); ++j) {
            if (suppressed[indices[j]]) continue;

            const Detection& det1 = detections[indices[i]];
            const Detection& det2 = detections[indices[j]];

            // 计算IoU
            cv::Rect intersection = det1.bbox & det2.bbox;
            float intersection_area = intersection.area();
            float union_area = det1.bbox.area() + det2.bbox.area() - intersection_area;
            float iou = intersection_area / union_area;

            if (iou > iou_threshold) {
                suppressed[indices[j]] = true;
            }
        }
    }

    return result;
}
