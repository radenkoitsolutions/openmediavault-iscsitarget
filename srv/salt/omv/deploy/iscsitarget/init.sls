# @license   https://www.gnu.org/licenses/gpl.html GPL Version 3
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# any later version.

include:
  - .{{ salt['pillar.get']('deploy_iscsitarget', 'default') }}
