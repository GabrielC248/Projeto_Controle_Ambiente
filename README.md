# Sistema de Controle para Conforto Térmico
  - **Desenvolvedor:** <ins>Gabriel Cavalcanti Coelho</ins>;
  - **Matrícula:** TIC370101612;
  - **Vídeo:** [YouTube](https://www.youtube.com/).

### Objetivos do projeto:
1. Monitorar continuamente a temperatura e a umidade do ambiente; 
2. Controlar automaticamente a velocidade do ventilador em três níveis 
conforme a temperatura; 
3. Acionar o umidificador quando a umidade estiver abaixo de um valor crítico; 
4. Alertar o usuário sobre a falta de água no umidificador por meio de 
indicadores visuais e sonoros; 
5. Exibir no display os valores medidos e o estado do sistema, incluindo um 
ícone facial para representar o nível de conforto térmico; 
6. Permitir a configuração personalizada dos limites de temperatura e de 
umidade. 

### Descrição do funcionamento:
O sistema utiliza o **joystick** da BitDogLab para simular sensores analógicos, o 
**eixo X** representa o **sensor de umidade** e o **eixo Y** representa o **sensor de 
temperatura**. O **LED RGB** representa o **estado dos dispositivos**, com o **LED 
vermelho** simulando as **velocidades do ventilador** por meio de PWM e o **LED azul** 
indicando o **funcionamento do umidificador**. O display exibe a temperatura, umidade, 
estado do ventilador e do umidificador, além de um rosto que muda a sua expressão 
conforme as condições simuladas. A matriz de LEDs indica a tela atual, que pode 
ser alternada pelo usuário entre a tela principal, a configuração das temperaturas, a 
configuração da umidade e a calibração do joystick, além de alertar sobre a 
necessidade de reabastecimento do umidificador. 
O sistema conta com o **botão do joystick** para **alternar entre as telas**, o **botão 
A** para **interagir com as telas de configurações** e o **botão B** para **simular o estado do 
sensor de nível do umidificador**, que indica se está com pouca água ou não. 
