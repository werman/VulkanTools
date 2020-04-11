/*
 * Copyright (c) 2020 Valve Corporation
 * Copyright (c) 2020 LunarG, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Author: Richard S. Wright Jr. <richard@lunarg.com>
 */


#include "dlgvulkaninfo.h"
#include "ui_dlgvulkaninfo.h"

#include <stdio.h>

#include <QProcess>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonObject>
#include <QJsonValue>
#include <QJsonArray>
#include <QMessageBox>
#include <QStringList>


dlgVulkanInfo::dlgVulkanInfo(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::dlgVulkanInfo)
{
    ui->setupUi(this);

    QProcess *vulkan_info = new QProcess(this);
#ifdef __APPLE__
    vulkan_info->setProgram("vulkaninfo");
#else
    vulkan_info->setProgram("vulkaninfo");
#endif


    QString filePath = QDir::temp().path() + "/vulkaninfo.json";

    QStringList args;
    args << "--vkconfig_output";
    args << QDir::temp().path();
    vulkan_info->setArguments(args);
    vulkan_info->start();
    vulkan_info->waitForFinished();

    QFile file(filePath);
     if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
         QMessageBox msgBox;
         msgBox.setText("Could not open vkconfig_output.json");
         msgBox.exec();
         return;
     }

     QString jsonText = file.readAll();
     file.close();

     //////////////////////////////////////////////////////
     // Convert the text to a JSON document & validate it
     QJsonDocument jsonDoc;
     QJsonParseError parseError;
     jsonDoc = QJsonDocument::fromJson(jsonText.toUtf8(), &parseError);
    if(parseError.error != QJsonParseError::NoError) {
         QMessageBox msgBox;
         msgBox.setText(parseError.errorString());
         msgBox.exec();
        return;
        }

    if(jsonDoc.isNull() || jsonDoc.isEmpty()) {
        QMessageBox msgBox;
        msgBox.setText(tr("Json document is empty!"));
        msgBox.exec();
        return;
        }


    /////////////////////////////////////////////////////////
    // Get the instance version and set that to the header
    QJsonObject jsonTopObject = jsonDoc.object();
    QJsonValue instance = jsonTopObject.value(QString("Vulkan Instance Version"));
    QString output = "Vulkan Instance Version: " + instance.toString();

    QTreeWidgetItem* header = ui->treeWidget->headerItem();
    header->setText(0, output);

    ////////////////////////////////////////////////////////////
    // Setp through each major section and parse.
    // All of these are the top layer nodes on the tree.
    QJsonValue rootObject = jsonTopObject.value(QString("Instance Extensions"));
    QTreeWidgetItem *pParentNode = new QTreeWidgetItem();
    pParentNode->setText(0, tr("Instance Extensions"));
    ui->treeWidget->addTopLevelItem(pParentNode);
    BuildExtensions(rootObject, pParentNode);

    rootObject = jsonTopObject.value("Layer Properties");
    pParentNode = new QTreeWidgetItem();
    BuildLayers(rootObject, pParentNode);

    rootObject = jsonTopObject.value("Presentable Surfaces");
    pParentNode = new QTreeWidgetItem();
    BuildSurfaces(rootObject, pParentNode);

    rootObject = jsonTopObject.value("Device Groups");
    pParentNode = new QTreeWidgetItem();
    BuildGroups(rootObject, pParentNode);

    rootObject = jsonTopObject.value(tr("Device Properties and Extensions"));
    pParentNode = new QTreeWidgetItem();
    pParentNode->setText(0, "Device Properties and Extensions");
    BuildDevices(rootObject, pParentNode);


}

dlgVulkanInfo::~dlgVulkanInfo()
{
    delete ui;
}


////////////////////////////////////////////////////////////////////////////////////
/// \brief TraverseGenericProperties
/// \param parentJson       - Top JSon node
/// \param pParentTreeItem  - Parent tree (GUI) item to add elements to
/// Many large sections are generic enough to simply parse and construct a tree,
/// without the need for any special formatting or extra text that is not in the
/// json file.
void dlgVulkanInfo::TraverseGenericProperties(QJsonValue& parentJson, QTreeWidgetItem *pParentTreeItem)
{
    QJsonObject parentObject = parentJson.toObject();
    int listSize = parentObject.size();
    QStringList fields = parentObject.keys();

    for(int field = 0; field < listSize; field++) {
        QJsonValue fieldValue = parentObject.value(fields[field]);

        if(!fieldValue.isArray()) {
            // Is it a single child or does it have children? If it has children, recurse
            QJsonObject childObject = fieldValue.toObject();
            if(childObject.size() > 0) {
                QTreeWidgetItem *pNewChild = new QTreeWidgetItem();
                pNewChild->setText(0, fields[field]);
                pParentTreeItem->addChild(pNewChild);
                TraverseGenericProperties(fieldValue, pNewChild);
                continue;
                }

            QTreeWidgetItem *pItem = new QTreeWidgetItem();
            pItem->setText(0, QString().sprintf("%s = %s", fields[field].toUtf8().constData(), fieldValue.toVariant().toString().toUtf8().constData()));
            pParentTreeItem->addChild(pItem);
            }
        else {
            // Add array list
            QJsonArray jsonArray = fieldValue.toArray();
            QTreeWidgetItem *pArrayParent = new QTreeWidgetItem();
            pArrayParent->setText(0, QString().sprintf("%s: count = %d", fields[field].toUtf8().constData(), jsonArray.size()));
            pParentTreeItem->addChild(pArrayParent);

            // The array is just a list of values. No children.
            for(int i = 0; i < jsonArray.size(); i++) {
                QTreeWidgetItem *pChild = new QTreeWidgetItem();
                // This looks weird... but integer fields will not conver to strings directly. However, if I
                // make them a variant first, then they do. I'd call this a Qt bug, but the word around is
                // a better choice than to wait for Qt to fix it and then have to require that version of Qt
                // to build this tool... RSW
                pChild->setText(0, jsonArray[i].toVariant().toString().toUtf8().constData());
                pArrayParent->addChild(pChild);
            }
        }
    }
}


////////////////////////////////////////////////////////////////////////////////////
/// \brief dlgVulkanInfo::BuildExtensions
/// \param jsonValue    - Top of tree
/// \param pRoot        - Root of GUI element for the tree
///
/// Populate the a subtree with extension names. Extensions also report their
/// spec version, so some extra text is needed, and thus the need for a special
/// function as opposed to just calling TraverseGenericProperties()
void dlgVulkanInfo::BuildExtensions(QJsonValue& jsonValue, QTreeWidgetItem *pRoot)
{    
    QString output;
    QJsonObject extensionObject = jsonValue.toObject();
    int nObjectSize = extensionObject.size();

    // Add all the extensions and thier version
    QStringList keys = extensionObject.keys();

    for(int i = 0; i < nObjectSize; i++) {
        QJsonValue value = extensionObject.value(keys[i]);
        QJsonObject object = value.toObject();
        QJsonValue specValue = object.value("specVersion");
        output = keys[i];
        output += " : extension revision ";
        output += specValue.toVariant().toString();

        QTreeWidgetItem *pSubItem = new QTreeWidgetItem();
        pSubItem->setText(0, output);
        pRoot->addChild(pSubItem);
    }
}

////////////////////////////////////////////////////////////////////////////////////
/// \brief dlgVulkanInfo::BuildLayers
/// \param jsonValue    - Root of Json tree
/// \param pRoot        - Root of GUI tree
/// This tree section has some different "kinds" of subtrees (the extensions)
/// and some extra text formatting requirements, so it had to be treated specially.
void dlgVulkanInfo::BuildLayers(QJsonValue& jsonValue, QTreeWidgetItem *pRoot)
{
    QJsonObject layersObject = jsonValue.toObject();
    int layersCount = layersObject.size();

    QString output = "Layers : count = ";
    output += QString().sprintf("%d", layersCount);

    pRoot->setText(0, output);
    ui->treeWidget->addTopLevelItem(pRoot);

   // Loop through all the layers
   QStringList layers = layersObject.keys();
   for(int i = 0; i < layersCount; i++) {
       QJsonValue layerTop = layersObject.value(layers[i]);
       QJsonObject layerObject = layerTop.toObject();
       output = layers[i] + " (";
       output += layerObject.value("description").toString();
       output += ") Vulkan version ";
       output += layerObject.value("version").toString();
       output += ", layer version ";
       output += layerObject.value("implementation version").toVariant().toString();

       QTreeWidgetItem *pLayer = new QTreeWidgetItem();
       pLayer->setText(0, output);

       // Each layer has extensions
       QJsonValue layerExtensions = layerObject.value("Layer Extensions");
       QJsonObject layerExtensionsObject = layerExtensions.toObject();
       int nExtCount = layerExtensionsObject.size();
       output.sprintf("Layer Extensions: count = %d", nExtCount);
       QTreeWidgetItem *pExtItem = new QTreeWidgetItem();
       pExtItem->setText(0, output);
       pLayer->addChild(pExtItem);

       BuildExtensions(layerExtensions, pExtItem);  // Generic enough

       // Each layer has devices too
        QJsonValue devicesValue = layerObject.value("Devices");
        QJsonObject devicesObject = devicesValue.toObject();
        int devCount = devicesObject.size();
        QTreeWidgetItem *pDeviceItem = new QTreeWidgetItem();
        pDeviceItem->setText(0, QString().sprintf("Devices: count = %d", devCount));
        pLayer->addChild(pDeviceItem);
        QStringList devicesList = devicesObject.keys();
        for(int dev = 0; dev < devCount; dev++) {
            QJsonValue gpuVal = devicesObject.value(devicesList[dev]);
            QJsonObject gpuObject = gpuVal.toObject();
            QJsonValue gpuIDValue = gpuObject.value("GPU id");
            output = "GPU id : ";
            output += gpuIDValue.toVariant().toString();
            output += " (";
            output += devicesList[dev];
            output += ")";
            QTreeWidgetItem *pNoChildren = new QTreeWidgetItem();
            pNoChildren->setText(0, output);
            pDeviceItem->addChild(pNoChildren);

            QJsonValue devExtVal = gpuObject.value("Layer-Device Extensions");
            QJsonObject devExtObj = devExtVal.toObject();
            int extCount = devExtObj.size();
            output.sprintf("Layer-Device Extensions: count = %d", extCount);
            QTreeWidgetItem *pNext = new QTreeWidgetItem();
            pNext->setText(0, output);
            pDeviceItem->addChild(pNext);
            BuildExtensions(devExtVal, pNext);  // Generic enough
        }
       pRoot->addChild(pLayer);
   }
}



///////////////////////////////////////////////////////////////////////////////////
/// \brief dlgVulkanInfo::BuildSurfaces
/// \param jsonValue        Json Root
/// \param pRoot            GUI tree root
///
/// Nice and well behaved. TraverseGenericProperties will build the whole tree.
void dlgVulkanInfo::BuildSurfaces(QJsonValue& jsonValue, QTreeWidgetItem *pRoot)
{
    QJsonObject surfaces = jsonValue.toObject();
    int surfaceCount = surfaces.size();
    QStringList GPUs = surfaces.keys();

    pRoot->setText(0, tr("Presentable Surfaces"));
    ui->treeWidget->addTopLevelItem(pRoot);

    for(int i = 0; i < surfaceCount; i++) {
        TraverseGenericProperties(jsonValue, pRoot);
    }
}

////////////////////////////////////////////////////////////////////////////////
/// \brief dlgVulkanInfo::BuildGroups
/// \param jsonValue        Json Root
/// \param pRoot            GUI tree root
///
/// Nice and well behaved. TraverseGenericProperties will build the whole tree.
void dlgVulkanInfo::BuildGroups(QJsonValue& jsonValue, QTreeWidgetItem *pRoot)
{
    QJsonObject groupsObject = jsonValue.toObject();
    pRoot->setText(0, tr("Device Groups"));
    ui->treeWidget->addTopLevelItem(pRoot);

    TraverseGenericProperties(jsonValue, pRoot);

}


////////////////////////////////////////////////////////////////////////////////
/// \brief dlgVulkanInfo::BuildDevices
/// \param jsonValue    Json Root
/// \param pRoot        GUI tree root
/// The Device Properties and Extensions tree is mostly pretty well behaved.
/// There is one section that can be handled by the TraverseGenericProperties()
/// function, and just one section that is specifially needing the
/// extensions list parser.
void dlgVulkanInfo::BuildDevices(QJsonValue& jsonValue, QTreeWidgetItem *pRoot)
{
    QJsonObject gpuObject = jsonValue.toObject();

    pRoot->setText(0, tr("Device Properties and Extensions"));
    ui->treeWidget->addTopLevelItem(pRoot);


    // For each like GPU0 object
    QStringList gpuList = gpuObject.keys();
    for(int i = 0; i < gpuObject.size(); i++) {
        QTreeWidgetItem *pGPU = new QTreeWidgetItem();
        pGPU->setText(0, gpuList[i]);
        pRoot->addChild(pGPU);

        QJsonValue properties = gpuObject.value(gpuList[i]);
        QJsonObject propertiesObject = properties.toObject();
        QStringList propertyParents = propertiesObject.keys();

        for(int j = 0; j < propertiesObject.size(); j++) {
            QTreeWidgetItem *pParent = new QTreeWidgetItem();
            pParent->setText(0, propertyParents[j]);
            pGPU->addChild(pParent);
            QJsonValue value = propertiesObject.value(propertyParents[j]);

            if(propertyParents[j] == QString("Device Extensions"))
                BuildExtensions(value, pParent);
            else
                TraverseGenericProperties(value, pParent);
            }
    }
}



//void dlgVulkanInfo::BuildDevices(QJsonValue& jsonValue, QTreeWidgetItem *pRoot)
//{
//    QJsonObject gpuObject = jsonValue.toObject();

//    pRoot->setText(0, tr("Device Properties and Extensions"));
//    ui->treeWidget->addTopLevelItem(pRoot);


//    // For each like GPU0 object
//    QStringList gpuList = gpuObject.keys();
//    for(int i = 0; i < gpuObject.size(); i++) {
//        QTreeWidgetItem *pGPU = new QTreeWidgetItem();
//        pGPU->setText(0, gpuList[i]);
//        pRoot->addChild(pGPU);

//        QJsonValue properties = gpuObject.value(gpuList[i]);
//        QJsonObject propertiesObject = properties.toObject();
//        QStringList propertyParents = propertiesObject.keys();

//        // Device Extensions needs special formating
//        QJsonValue deviceExtensionsValue = propertiesObject.value("Device Extensions");
//        QJsonObject deviceExtensionsObject = deviceExtensionsValue.toObject();
//        QStringList deviceExtensionKeys = deviceExtensionsObject.keys();
//        QTreeWidgetItem *pDeviceExtensions = new QTreeWidgetItem();
//        pDeviceExtensions->setText(0, "Device Extensions");
//        pGPU->addChild(pDeviceExtensions);
//        BuildExtensions(deviceExtensionsValue, pDeviceExtensions);

//        // Okay, there's a whole block of these that are identical and generic
//        static const char *Nodes[70] = { "Format Properties", "VkPhysicalDeviceProperties", "VkPhysicalDeviceLimits",
//                               "VkPhysicalDeviceSparseProperties", "VkPhysicalDeviceBlendOperationAdvancedPropertiesEXT",
//                               "VkPhysicalDeviceConservativeRasterizationPropertiesEXT", "VkPhysicalDeviceDepthStencilResolveProperties",
//                               "VkPhysicalDeviceDescriptorIndexingProperties", "VkPhysicalDeviceDiscardRectanglePropertiesEXT",
//                               "VkPhysicalDeviceDriverProperties", "VkPhysicalDeviceFloatControlsProperties", "VkPhysicalDeviceIDProperties",
//                               "VkPhysicalDeviceInlineUniformBlockPropertiesEXT", "VkPhysicalDeviceLineRasterizationPropertiesEXT",
//                               "VkPhysicalDeviceMaintenance3Properties", "VkPhysicalDeviceMultiviewProperties", "VkPhysicalDevicePCIBusInfoPropertiesEXT",
//                               "VkPhysicalDevicePointClippingProperties", "VkPhysicalDeviceProtectedMemoryProperties", "VkPhysicalDevicePushDescriptorPropertiesKHR",
//                               "VkPhysicalDeviceSampleLocationsPropertiesEXT", "VkPhysicalDeviceSamplerFilterMinmaxProperties",
//                               "VkPhysicalDeviceSubgroupProperties", "VkPhysicalDeviceSubgroupSizeControlPropertiesEXT", "VkPhysicalDeviceTexelBufferAlignmentPropertiesEXT",
//                               "VkPhysicalDeviceTimelineSemaphoreProperties", "VkPhysicalDeviceTransformFeedbackPropertiesEXT", "VkPhysicalDeviceVertexAttributeDivisorPropertiesEXT",
//                               "VkPhysicalDeviceVulkan11Properties", "VkPhysicalDeviceVulkan12Properties", "VkQueueFamilyProperties", "VkPhysicalDeviceMemoryProperties",
//                               "VkPhysicalDeviceFeatures", "VkPhysicalDevice16BitStorageFeatures", "VkPhysicalDevice8BitStorageFeatures", "VkPhysicalDeviceBlendOperationAdvancedFeaturesEXT",
//                               "VkPhysicalDeviceBufferDeviceAddressFeatures", "VkPhysicalDeviceBufferDeviceAddressFeaturesEXT", "VkPhysicalDeviceConditionalRenderingFeaturesEXT",
//                               "VkPhysicalDeviceDepthClipEnableFeaturesEXT", "VkPhysicalDeviceDescriptorIndexingFeatures", "VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT",
//                               "VkPhysicalDeviceHostQueryResetFeatures", "VkPhysicalDeviceImagelessFramebufferFeatures", "VkPhysicalDeviceIndexTypeUint8FeaturesEXT",
//                               "VkPhysicalDeviceInlineUniformBlockFeaturesEXT", "VkPhysicalDeviceLineRasterizationFeaturesEXT", "VkPhysicalDeviceMultiviewFeatures",
//                               "VkPhysicalDevicePipelineExecutablePropertiesFeaturesKHR", "VkPhysicalDeviceProtectedMemoryFeatures", "VkPhysicalDeviceSamplerYcbcrConversionFeatures",
//                               "VkPhysicalDeviceScalarBlockLayoutFeatures", "VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures", "VkPhysicalDeviceShaderAtomicInt64Features",
//                               "VkPhysicalDeviceShaderClockFeaturesKHR", "VkPhysicalDeviceShaderDemoteToHelperInvocationFeaturesEXT", "VkPhysicalDeviceShaderDrawParametersFeatures",
//                               "VkPhysicalDeviceShaderFloat16Int8Features", "VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures", "VkPhysicalDeviceSubgroupSizeControlFeaturesEXT",
//                               "VkPhysicalDeviceTexelBufferAlignmentFeaturesEXT", "VkPhysicalDeviceTimelineSemaphoreFeatures", "VkPhysicalDeviceTransformFeedbackFeaturesEXT",
//                               "VkPhysicalDeviceUniformBufferStandardLayoutFeatures", "VkPhysicalDeviceVariablePointersFeatures", "VkPhysicalDeviceVertexAttributeDivisorFeaturesEXT",
//                               "VkPhysicalDeviceVulkan11Features", "VkPhysicalDeviceVulkan12Features", "VkPhysicalDeviceVulkanMemoryModelFeatures", "VkPhysicalDeviceYcbcrImageArraysFeaturesEXT",
//                               };

//        for(unsigned int node = 0; node < 70; node++) {
//            QJsonValue nodeValue = propertiesObject.value(Nodes[node]);
//            QTreeWidgetItem* pItem = new QTreeWidgetItem();
//            pItem->setText(0, Nodes[node]);
//            pGPU->addChild(pItem);
//            TraverseGenericProperties(nodeValue, pItem);
//        }
//    }
//}
