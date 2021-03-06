# -*- coding: utf-8 -*-

"""
***************************************************************************
    lasinfo.py
    ---------------------
    Date                 : August 2012
    Copyright            : (C) 2012 by Victor Olaya
    Email                : volayaf at gmail dot com
    ---------------------
    Date                 : March 2014
    Copyright            : (C) 2014 by Martin Isenburg
    Email                : martin near rapidlasso point com
***************************************************************************
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
***************************************************************************
"""
from future import standard_library
standard_library.install_aliases()
from builtins import str

__author__ = 'Martin Isenburg'
__date__ = 'March 2014'
__copyright__ = '(C) 2014, Martin Isenburg'
# This will get replaced with a git SHA1 when you do a git archive
__revision__ = '$Format:%H$'

import os
from .LAStoolsUtils import LAStoolsUtils
from processing.core.parameters import ParameterExtent
from .LAStoolsAlgorithm import LAStoolsAlgorithm
from qgis.core import QgsMapLayer, QgsProject


class lasquery(LAStoolsAlgorithm):

    AOI = "AOI"

    def defineCharacteristics(self):
        self.name, self.i18n_name = self.trAlgorithm('lasquery')
        self.group, self.i18n_group = self.trAlgorithm('LAStools')
        self.addParametersVerboseGUI()
        self.addParameter(ParameterExtent(self.AOI, self.tr('area of interest'), optional=False))
        self.addParametersAdditionalGUI()

    def processAlgorithm(self, progress):

        commands = [os.path.join(LAStoolsUtils.LAStoolsPath(), "bin", "lasview")]
        self.addParametersVerboseCommands(commands)

    # get area-of-interest
        aoi = str(self.getParameterValue(self.AOI))
        aoiCoords = aoi.split(',')

        # get layers
        layers = QgsProject.instance().mapLayers()

        # loop over layers
        for name, layer in list(layers.items()):
            layerType = layer.type()
            if layerType == QgsMapLayer.VectorLayer:
                shp_file_name = layer.source()
                file_name = shp_file_name[:-4] + ".laz"
                commands.append('-i')
                commands.append(file_name)

        commands.append("-files_are_flightlines")
        commands.append('-inside')
        commands.append(aoiCoords[0])
        commands.append(aoiCoords[2])
        commands.append(aoiCoords[1])
        commands.append(aoiCoords[3])
        self.addParametersAdditionalCommands(commands)

        LAStoolsUtils.runLAStools(commands, progress)
