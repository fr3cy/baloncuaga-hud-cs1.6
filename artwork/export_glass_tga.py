"""Required size/format export of the code-native rounded glass UI vector."""
from pathlib import Path
from PIL import Image
import struct
root=Path(__file__).resolve().parent.parent
image=Image.open(root/'artwork/glass_scoreboard.png').convert('RGBA')
image=image.resize((1024,670),Image.Resampling.LANCZOS)
header=struct.pack('<BBBHHBHHHHBB',0,0,2,0,0,0,0,0,image.width,image.height,32,8)
data=header+image.transpose(Image.Transpose.FLIP_TOP_BOTTOM).tobytes('raw','BGRA')+b'\0'*8+b'TRUEVISION-XFILE.\0'
for module in ['cstrike','valve']:
    target=root/'game_skin'/module/'gfx/vgui/glass_scoreboard.tga'
    target.parent.mkdir(parents=True,exist_ok=True)
    target.write_bytes(data)
    assert Image.open(target).convert('RGBA').tobytes()==image.tobytes()
print('Exported 1024x670 RGBA glass UI backplate for both native resource roots.')
